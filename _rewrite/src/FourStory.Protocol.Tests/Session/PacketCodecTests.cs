using System.Buffers.Binary;
using FourStory.Protocol;
using FourStory.Protocol.Crypto;
using FourStory.Protocol.Session;

namespace FourStory.Protocol.Tests.Session;

public class PacketCodecTests
{
    /// <summary>
    /// A "client codec" used in tests to produce a packet exactly as the legacy C++ client would,
    /// so we can verify the server-side codec accepts it.
    /// </summary>
    private static byte[] BuildClientPacket(uint sendNumber, ushort messageId, ReadOnlySpan<byte> body)
    {
        var totalSize = PacketHeader.Size + body.Length;
        var packet = new byte[totalSize];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)totalSize);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), messageId);
        BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(4, 4), sendNumber);
        body.CopyTo(packet.AsSpan(PacketHeader.Size));

        // Client outbound: XOR body, XOR header, then RC4 the WHOLE packet but keep wSize
        // plaintext on the wire (matches legacy C++ Session.cpp:88 keystream alignment).
        var key = KeyTable.KeyFor(sendNumber);
        XorLayer.EncryptBody(packet, key);
        XorLayer.EncryptHeader(packet, key);
        Rc4Layer.TransformPacketPreservingWSize(packet);

        return packet;
    }

    [Fact]
    public void ServerCodec_DecryptsClientCraftedPacket_WithSequenceAndChecksum()
    {
        var body = new byte[] { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
        var packet = BuildClientPacket(sendNumber: 1, messageId: 0x1988, body);

        var codec = new PacketCodec(PeerType.Client) { CryptEnabled = true };
        var result = codec.TryDecrypt(packet);

        Assert.Equal(PacketDecryptResult.Ok, result);
        Assert.Equal(1u, codec.RecvNumber);
        Assert.Equal(0x1988, BinaryPrimitives.ReadUInt16LittleEndian(packet.AsSpan(2, 2)));
        Assert.Equal(body, packet.AsSpan(PacketHeader.Size).ToArray());
    }

    [Fact]
    public void ServerCodec_RejectsOutOfOrderSequence()
    {
        // First packet is sendNumber=1 but we craft one with sendNumber=5.
        var body = new byte[] { 42 };
        var packet = BuildClientPacket(sendNumber: 5, messageId: 0x1988, body);

        var codec = new PacketCodec(PeerType.Client) { CryptEnabled = true };
        var result = codec.TryDecrypt(packet);

        Assert.Equal(PacketDecryptResult.SequenceMismatch, result);
        Assert.Equal(0u, codec.RecvNumber); // counter must NOT advance on rejection
    }

    [Fact]
    public void ServerCodec_RejectsTamperedBody()
    {
        var body = new byte[] { 1, 2, 3, 4, 5, 6, 7, 8 };
        var packet = BuildClientPacket(sendNumber: 1, messageId: 0x1988, body);

        // Tamper a body byte AFTER full client-side encryption.
        packet[^1] ^= 0xFF;

        var codec = new PacketCodec(PeerType.Client) { CryptEnabled = true };
        var result = codec.TryDecrypt(packet);

        Assert.NotEqual(PacketDecryptResult.Ok, result);
    }

    [Fact]
    public void ServerCodec_RoundTrip_OutboundDecodedByMirrorCodec()
    {
        // Simulate: server prepares outbound packet, then a "client-side" decoder reverses it.
        // Server-to-client is XOR-only (no RC4 on outbound from server).
        var body = "Hello world from server"u8.ToArray();
        var totalSize = PacketHeader.Size + body.Length;
        var packet = new byte[totalSize];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)totalSize);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), 0x1989); // CS_LOGIN_ACK
        body.CopyTo(packet.AsSpan(PacketHeader.Size));

        var serverCodec = new PacketCodec(PeerType.Client) { CryptEnabled = true };
        serverCodec.Encrypt(packet);

        Assert.Equal(1u, serverCodec.SendNumber);
        Assert.Equal((ushort)totalSize, BinaryPrimitives.ReadUInt16LittleEndian(packet.AsSpan(0, 2))); // wSize plaintext

        // Reverse on "client" side: XOR header + body decrypt with key from dwNumber.
        var seq = BinaryPrimitives.ReadUInt32LittleEndian(packet.AsSpan(4, 4));
        // dwNumber is still encrypted at this point — header XOR mask depends on plaintext wSize/wId,
        // so we decrypt header first which reveals dwNumber. Use sendNumber from the codec since
        // we know it should be 1; the key is deterministic from that.
        var key = KeyTable.KeyFor(serverCodec.SendNumber);
        XorLayer.DecryptHeader(packet, key);
        var seqAfter = BinaryPrimitives.ReadUInt32LittleEndian(packet.AsSpan(4, 4));
        Assert.Equal(serverCodec.SendNumber, seqAfter);

        Assert.True(XorLayer.DecryptBody(packet, key));
        Assert.Equal(body, packet.AsSpan(PacketHeader.Size).ToArray());
    }

    [Fact]
    public void Plaintext_WhenCryptDisabled_PacketUnchanged()
    {
        var body = new byte[] { 1, 2, 3, 4 };
        var totalSize = PacketHeader.Size + body.Length;
        var packet = new byte[totalSize];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)totalSize);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), 0x9001);
        body.CopyTo(packet.AsSpan(PacketHeader.Size));
        var original = packet.ToArray();

        var codec = new PacketCodec(PeerType.Server); // server-to-server, crypt off
        codec.Encrypt(packet);
        Assert.Equal(original, packet);

        var result = codec.TryDecrypt(packet);
        Assert.Equal(PacketDecryptResult.Ok, result);
        Assert.Equal(original, packet);
    }

    [Fact]
    public void Encrypt_WithMismatchedWSize_Throws()
    {
        var packet = new byte[20];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), 99); // claims 99 but buffer is 20

        var codec = new PacketCodec(PeerType.Client) { CryptEnabled = true };
        Assert.Throws<ArgumentException>(() => codec.Encrypt(packet));
    }
}
