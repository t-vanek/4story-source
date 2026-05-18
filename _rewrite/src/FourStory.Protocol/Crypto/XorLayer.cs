using System.Buffers.Binary;
using System.Runtime.CompilerServices;

namespace FourStory.Protocol.Crypto;

/// <summary>
/// XOR + INT64 checksum body cipher, plus header obfuscation.
/// Direct port of <c>CPacket::Encrypt/Decrypt/EncryptHeader/DecryptHeader</c> from
/// <c>Server/TNetLib/Packet.cpp</c>.
/// </summary>
public static class XorLayer
{
    /// <summary>
    /// Encrypts the body (bytes after header) in place and writes the plaintext-checksum
    /// into <paramref name="packet"/>[8..16] (the <c>llChkSUM</c> field).
    /// </summary>
    public static void EncryptBody(Span<byte> packet, long key)
    {
        ValidatePacket(packet);

        long checksum = 0;
        var body = packet[PacketHeader.Size..];
        var bodyLen = body.Length;
        var fullChunks = bodyLen / 8;
        var remainder = bodyLen % 8;

        // INT64-aligned chunks: XOR with key, checksum over plaintext.
        for (var i = 0; i < fullChunks; i++)
        {
            var slice = body.Slice(i * 8, 8);
            var plain = BinaryPrimitives.ReadInt64LittleEndian(slice);
            checksum ^= plain;
            BinaryPrimitives.WriteInt64LittleEndian(slice, plain ^ key);
        }

        // Tail bytes: XOR with corresponding key byte, plus rolling CRC contribution.
        if (remainder > 0)
        {
            var tail = body[(fullChunks * 8)..];
            long crc = 0;
            for (var i = 0; i < remainder; i++)
            {
                var keyByte = (byte)(key >> (i * 8));
                var plain = tail[i];
                checksum ^= plain;
                tail[i] = (byte)(plain ^ keyByte);
                crc = ((crc >> 4) & 0x0FFDL) ^ key;
                checksum += crc;
            }
        }

        // Store checksum into header.llChkSUM (offset 8).
        BinaryPrimitives.WriteInt64LittleEndian(packet.Slice(8, 8), checksum);
    }

    /// <summary>
    /// Decrypts the body and verifies the plaintext checksum.
    /// Returns true if the checksum matches, false otherwise.
    /// </summary>
    public static bool DecryptBody(Span<byte> packet, long key)
    {
        ValidatePacket(packet);

        var expectedChecksum = BinaryPrimitives.ReadInt64LittleEndian(packet.Slice(8, 8));
        long computedChecksum = 0;
        var body = packet[PacketHeader.Size..];
        var bodyLen = body.Length;
        var fullChunks = bodyLen / 8;
        var remainder = bodyLen % 8;

        for (var i = 0; i < fullChunks; i++)
        {
            var slice = body.Slice(i * 8, 8);
            var cipher = BinaryPrimitives.ReadInt64LittleEndian(slice);
            var plain = cipher ^ key;
            BinaryPrimitives.WriteInt64LittleEndian(slice, plain);
            computedChecksum ^= plain;
        }

        if (remainder > 0)
        {
            var tail = body[(fullChunks * 8)..];
            long crc = 0;
            for (var i = 0; i < remainder; i++)
            {
                var keyByte = (byte)(key >> (i * 8));
                tail[i] = (byte)(tail[i] ^ keyByte);
                computedChecksum ^= tail[i];
                crc = ((crc >> 4) & 0x0FFDL) ^ key;
                computedChecksum += crc;
            }
        }

        return computedChecksum == expectedChecksum;
    }

    /// <summary>
    /// Obfuscates bytes [2..16] of the header (everything except <c>wSize</c>) using the key
    /// and the plaintext wSize / wId values. This is symmetric — same operation decrypts.
    /// </summary>
    public static void TransformHeader(Span<byte> packet, long key)
    {
        ValidatePacket(packet);
        var header = packet[..PacketHeader.Size];

        // Snapshot the wSize and wId fields BEFORE we touch any header bytes —
        // the C++ code stashes wID in a local before iterating.
        var wSize = BinaryPrimitives.ReadUInt16LittleEndian(header[..2]);
        var wId = BinaryPrimitives.ReadUInt16LittleEndian(header.Slice(2, 2));

        // Loop over bytes 2..15 (i = 0..13). i<2 uses wSize, i>=2 uses wId.
        for (var i = 0; i < PacketHeader.Size - 2; i++)
        {
            byte mask;
            if (i < 2)
            {
                mask = (byte)(key + wSize + i);
            }
            else
            {
                mask = (byte)(key + wId + i);
            }
            header[2 + i] ^= mask;
        }
    }

    /// <inheritdoc cref="TransformHeader"/>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void EncryptHeader(Span<byte> packet, long key) => TransformHeader(packet, key);

    /// <summary>
    /// Decrypts the header in place. NOTE: unlike <see cref="EncryptHeader"/> which sees
    /// the plaintext wId before the loop, here the wId we need is the plaintext one — and on
    /// receive that's only available AFTER bytes 0..1 of the wId field (= header[2..4])
    /// have been XORed back. The C++ code handles this by reading <c>m_pHeader-&gt;m_wID</c>
    /// each iteration, which after the first two iterations contains plaintext. We replicate
    /// that exact byte-by-byte behavior here.
    /// </summary>
    public static void DecryptHeader(Span<byte> packet, long key)
    {
        ValidatePacket(packet);
        var header = packet[..PacketHeader.Size];

        // wSize is always plaintext on the wire — read once.
        var wSize = BinaryPrimitives.ReadUInt16LittleEndian(header[..2]);

        for (var i = 0; i < PacketHeader.Size - 2; i++)
        {
            byte mask;
            if (i < 2)
            {
                mask = (byte)(key + wSize + i);
            }
            else
            {
                // For i>=2, the C++ code reads m_pHeader->m_wID, which by now has been
                // decrypted in iterations i=0 and i=1 above. Re-read the in-place value.
                var wId = BinaryPrimitives.ReadUInt16LittleEndian(header.Slice(2, 2));
                mask = (byte)(key + wId + i);
            }
            header[2 + i] ^= mask;
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private static void ValidatePacket(Span<byte> packet)
    {
        if (packet.Length < PacketHeader.Size)
        {
            throw new ArgumentException(
                $"Packet must be at least {PacketHeader.Size} bytes (header), got {packet.Length}.",
                nameof(packet));
        }
    }
}
