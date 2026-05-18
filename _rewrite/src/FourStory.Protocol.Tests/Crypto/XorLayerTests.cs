using System.Buffers.Binary;
using FourStory.Protocol;
using FourStory.Protocol.Crypto;

namespace FourStory.Protocol.Tests.Crypto;

public class XorLayerTests
{
    [Fact]
    public void EncryptBody_DecryptBody_RoundTrip_RestoresPlaintext()
    {
        // Build a packet with 16B header + 24B body (3 full INT64 chunks).
        var packet = new byte[16 + 24];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)packet.Length);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), 0x1988); // CS_LOGIN_REQ
        BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(4, 4), 1);     // dwNumber

        // Plaintext body: 24 known bytes.
        for (var i = 0; i < 24; i++)
        {
            packet[16 + i] = (byte)(i + 1);
        }
        var originalBody = packet.AsSpan(16).ToArray();

        var key = KeyTable.KeyFor(1);

        XorLayer.EncryptBody(packet, key);

        // After encrypt, body must differ from plaintext (probabilistically true for non-zero key).
        Assert.NotEqual(originalBody, packet.AsSpan(16).ToArray());
        // Checksum must be non-zero (plaintext sum was non-zero).
        var checksum = BinaryPrimitives.ReadInt64LittleEndian(packet.AsSpan(8, 8));
        Assert.NotEqual(0L, checksum);

        // Decrypt should restore body AND validate checksum.
        var checksumOk = XorLayer.DecryptBody(packet, key);
        Assert.True(checksumOk);
        Assert.Equal(originalBody, packet.AsSpan(16).ToArray());
    }

    [Fact]
    public void EncryptBody_DecryptBody_HandlesPartialTailBytes()
    {
        // Body = 11 bytes = 1 full INT64 chunk + 3 remainder bytes.
        var packet = new byte[16 + 11];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)packet.Length);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), 0x1988);
        BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(4, 4), 5);

        for (var i = 0; i < 11; i++)
        {
            packet[16 + i] = (byte)(0xA0 | i);
        }
        var originalBody = packet.AsSpan(16).ToArray();
        var key = KeyTable.KeyFor(5);

        XorLayer.EncryptBody(packet, key);
        Assert.True(XorLayer.DecryptBody(packet, key));
        Assert.Equal(originalBody, packet.AsSpan(16).ToArray());
    }

    [Fact]
    public void DecryptBody_TamperedBody_FailsChecksum()
    {
        var packet = new byte[16 + 16];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)packet.Length);
        for (var i = 0; i < 16; i++)
        {
            packet[16 + i] = (byte)i;
        }
        var key = KeyTable.KeyFor(2);
        XorLayer.EncryptBody(packet, key);

        // Tamper with one byte.
        packet[20] ^= 0xFF;

        var ok = XorLayer.DecryptBody(packet, key);
        Assert.False(ok);
    }

    [Fact]
    public void EncryptHeader_DecryptHeader_RoundTrip_RestoresHeader()
    {
        var packet = new byte[16 + 8];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)packet.Length);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), 0x5281); // CS_MAP+1
        BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(4, 4), 0x12345678);
        BinaryPrimitives.WriteInt64LittleEndian(packet.AsSpan(8, 8), unchecked((long)0xFEEDFACECAFEBEEFUL));
        var original = packet.ToArray();
        var key = KeyTable.KeyFor(7);

        XorLayer.EncryptHeader(packet, key);
        // wSize must NOT change (framing depends on it being plaintext).
        Assert.Equal(original[0], packet[0]);
        Assert.Equal(original[1], packet[1]);
        // wId AND following fields should have changed.
        Assert.NotEqual(original.AsSpan(2, 14).ToArray(), packet.AsSpan(2, 14).ToArray());

        XorLayer.DecryptHeader(packet, key);
        Assert.Equal(original, packet);
    }

    [Theory]
    [InlineData(0u)]
    [InlineData(1u)]
    [InlineData(6u)]
    [InlineData(7u)]
    [InlineData(13u)]
    [InlineData(100u)]
    public void KeyTable_KeyFor_MatchesModulo7(uint number)
    {
        Assert.Equal(KeyTable.Keys[(int)(number % KeyTable.Count)], KeyTable.KeyFor(number));
    }
}
