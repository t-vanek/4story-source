using System.Buffers.Binary;
using FourStory.Protocol.Crypto;

namespace FourStory.Protocol.Session;

/// <summary>
/// Server-side per-session codec. Mirrors the orchestration in
/// <c>Server/TNetLib/Session.cpp</c>'s <c>Encrypt</c> / <c>Decrypt</c>.
///
/// Outbound (server → peer):
///   1. Increment sendNumber, write into header.
///   2. Apply <see cref="XorLayer"/> body encrypt + header obfuscation with key = g_4skey[dwNumber % 7].
///   3. No RC4 on outbound (asymmetric — only client→server gets the outer RC4 layer).
///
/// Inbound (peer → server):
///   1. Read wSize plaintext (framing layer — out of scope here).
///   2. If peer is a Client: RC4-decrypt the packet first.
///   3. XOR-decrypt header (skipping wSize).
///   4. Verify dwNumber == expected (recvNumber + 1). Reject if not.
///   5. XOR-decrypt body, validate checksum.
/// </summary>
public sealed class PacketCodec
{
    private uint _sendNumber;
    private uint _recvNumber;

    public PacketCodec(PeerType peerType)
    {
        PeerType = peerType;
    }

    public PeerType PeerType { get; }

    /// <summary>
    /// When false (default for fresh sessions), packets cross the wire in plaintext.
    /// Toggled to true by the server after the login handshake completes (see TLoginSvr's
    /// post-auth path). Server↔server sessions typically stay false.
    /// </summary>
    public bool CryptEnabled { get; set; }

    public uint SendNumber => _sendNumber;
    public uint RecvNumber => _recvNumber;

    /// <summary>
    /// Encrypts a fully-populated outbound packet in place (header + body).
    /// Caller must have already written <c>wSize</c>, <c>wID</c>, and the payload —
    /// this method fills in <c>dwNumber</c> and <c>llChkSUM</c> via the XOR pass.
    /// </summary>
    public void Encrypt(Span<byte> packet)
    {
        EnsureValidPacket(packet);
        if (!CryptEnabled)
        {
            return;
        }

        _sendNumber++;
        BinaryPrimitives.WriteUInt32LittleEndian(packet.Slice(4, 4), _sendNumber);

        var key = KeyTable.KeyFor(_sendNumber);
        XorLayer.EncryptBody(packet, key);
        XorLayer.EncryptHeader(packet, key);

        // Outbound path is intentionally one-sided: no RC4. The legacy client decrypts
        // server messages with only the XOR layer (Session.cpp Encrypt() has no RC4 call).
    }

    /// <summary>
    /// Decrypts a fully-received inbound packet in place. Returns <see cref="PacketDecryptResult"/>
    /// indicating success or the specific reason for rejection.
    /// </summary>
    public PacketDecryptResult TryDecrypt(Span<byte> packet)
    {
        EnsureValidPacket(packet);
        if (!CryptEnabled)
        {
            return PacketDecryptResult.Ok;
        }

        var expectedNumber = _recvNumber + 1;
        var key = KeyTable.KeyFor(expectedNumber);

        if (PeerType == PeerType.Client)
        {
            // RC4 spans the entire packet INCLUDING the wSize bytes — the keystream
            // alignment must match the legacy C++ server (Session.cpp:88), which calls
            // EncryptBuffer(CALG_RC4, m_pBuf, wSize, ...) over the whole packet and then
            // restores m_pHeader->m_wSize from a local. See COMPLETENESS_ANALYSIS.md §1.
            Rc4Layer.TransformPacketPreservingWSize(packet);
        }

        XorLayer.DecryptHeader(packet, key);

        var wireNumber = BinaryPrimitives.ReadUInt32LittleEndian(packet.Slice(4, 4));
        if (wireNumber != expectedNumber)
        {
            return PacketDecryptResult.SequenceMismatch;
        }

        if (!XorLayer.DecryptBody(packet, key))
        {
            return PacketDecryptResult.ChecksumMismatch;
        }

        _recvNumber = expectedNumber;
        return PacketDecryptResult.Ok;
    }

    private static void EnsureValidPacket(Span<byte> packet)
    {
        if (packet.Length < PacketHeader.Size)
        {
            throw new ArgumentException(
                $"Packet too small: {packet.Length} bytes, need at least {PacketHeader.Size}.",
                nameof(packet));
        }
        var wSize = BinaryPrimitives.ReadUInt16LittleEndian(packet[..2]);
        if (wSize != packet.Length)
        {
            throw new ArgumentException(
                $"Packet wSize ({wSize}) does not match buffer length ({packet.Length}).",
                nameof(packet));
        }
    }
}

public enum PacketDecryptResult
{
    Ok,
    SequenceMismatch,
    ChecksumMismatch,
}
