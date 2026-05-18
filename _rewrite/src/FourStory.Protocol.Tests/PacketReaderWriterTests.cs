using FourStory.Protocol;

namespace FourStory.Protocol.Tests;

public class PacketReaderWriterTests
{
    [Fact]
    public void PrimitiveRoundTrip_PreservesValues()
    {
        Span<byte> buffer = stackalloc byte[256];
        var w = new PacketWriter(buffer);

        w.WriteByte(0xAB);
        w.WriteSByte(-5);
        w.WriteUInt16(0xBEEF);
        w.WriteInt16(-12345);
        w.WriteUInt32(0xDEADBEEF);
        w.WriteInt32(-987654321);
        w.WriteInt64(unchecked((long)0xFEEDFACECAFEBEEFUL));
        w.WriteSingle(3.14159f);
        w.WriteString("Hello");
        w.WriteString(string.Empty);
        w.WriteBlob([0x01, 0x02, 0x03, 0x04]);

        var written = w.WrittenSpan.ToArray();
        var r = new PacketReader(written);

        Assert.Equal(0xAB, r.ReadByte());
        Assert.Equal(-5, r.ReadSByte());
        Assert.Equal(0xBEEF, r.ReadUInt16());
        Assert.Equal(-12345, r.ReadInt16());
        Assert.Equal(0xDEADBEEF, r.ReadUInt32());
        Assert.Equal(-987654321, r.ReadInt32());
        Assert.Equal(unchecked((long)0xFEEDFACECAFEBEEFUL), r.ReadInt64());
        Assert.Equal(3.14159f, r.ReadSingle());
        Assert.Equal("Hello", r.ReadString());
        Assert.Equal(string.Empty, r.ReadString());
        Assert.Equal(new byte[] { 0x01, 0x02, 0x03, 0x04 }, r.ReadBlob());
        Assert.True(r.IsEof);
    }

    [Fact]
    public void String_IsLittleEndianLengthPrefixed_Cp1252Bytes()
    {
        // Confirms wire format: int32 LE length + raw CP1252 bytes (no NUL terminator).
        Span<byte> buffer = stackalloc byte[64];
        var w = new PacketWriter(buffer);
        w.WriteString("AB");

        var written = w.WrittenSpan.ToArray();
        // 4 bytes length (=2) + 2 bytes "AB"
        Assert.Equal(new byte[] { 0x02, 0x00, 0x00, 0x00, (byte)'A', (byte)'B' }, written);
    }

    [Fact]
    public void Reader_UnderflowThrows()
    {
        // ref struct can't be captured in a lambda — use a helper method.
        static void ReadFromTooSmall()
        {
            var r = new PacketReader(new byte[] { 0x01 });
            r.ReadUInt32();
        }
        Assert.Throws<InvalidOperationException>(ReadFromTooSmall);
    }
}
