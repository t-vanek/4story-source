using System.Buffers.Binary;
using System.Runtime.CompilerServices;
using FourStory.Protocol.Encoding;

namespace FourStory.Protocol;

/// <summary>
/// Writes primitives to a packet payload in legacy little-endian wire format.
/// Mirrors the C++ <c>CPacket::operator&lt;&lt;</c> chain.
/// </summary>
public ref struct PacketWriter
{
    private readonly Span<byte> _buffer;
    private int _position;

    public PacketWriter(Span<byte> buffer)
    {
        _buffer = buffer;
        _position = 0;
    }

    public readonly int Position => _position;
    public readonly int Capacity => _buffer.Length;
    public readonly Span<byte> WrittenSpan => _buffer[.._position];

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void EnsureCanWrite(int count)
    {
        if (_position + count > _buffer.Length)
        {
            throw new InvalidOperationException(
                $"Packet overflow: tried to write {count} bytes at position {_position}, capacity {_buffer.Length}.");
        }
    }

    public void WriteByte(byte v)
    {
        EnsureCanWrite(1);
        _buffer[_position++] = v;
    }

    public void WriteSByte(sbyte v) => WriteByte(unchecked((byte)v));

    public void WriteUInt16(ushort v)
    {
        EnsureCanWrite(2);
        BinaryPrimitives.WriteUInt16LittleEndian(_buffer.Slice(_position, 2), v);
        _position += 2;
    }

    public void WriteInt16(short v)
    {
        EnsureCanWrite(2);
        BinaryPrimitives.WriteInt16LittleEndian(_buffer.Slice(_position, 2), v);
        _position += 2;
    }

    public void WriteUInt32(uint v)
    {
        EnsureCanWrite(4);
        BinaryPrimitives.WriteUInt32LittleEndian(_buffer.Slice(_position, 4), v);
        _position += 4;
    }

    public void WriteInt32(int v)
    {
        EnsureCanWrite(4);
        BinaryPrimitives.WriteInt32LittleEndian(_buffer.Slice(_position, 4), v);
        _position += 4;
    }

    public void WriteInt64(long v)
    {
        EnsureCanWrite(8);
        BinaryPrimitives.WriteInt64LittleEndian(_buffer.Slice(_position, 8), v);
        _position += 8;
    }

    public void WriteUInt64(ulong v)
    {
        EnsureCanWrite(8);
        BinaryPrimitives.WriteUInt64LittleEndian(_buffer.Slice(_position, 8), v);
        _position += 8;
    }

    public void WriteSingle(float v)
    {
        EnsureCanWrite(4);
        BinaryPrimitives.WriteSingleLittleEndian(_buffer.Slice(_position, 4), v);
        _position += 4;
    }

    /// <summary>Writes length-prefixed CP1252 string (matches C++ <c>operator&lt;&lt;(LPCTSTR)</c>).</summary>
    public void WriteString(string value)
    {
        if (string.IsNullOrEmpty(value))
        {
            WriteInt32(0);
            return;
        }
        var byteCount = Cp1252.Instance.GetByteCount(value);
        WriteInt32(byteCount);
        EnsureCanWrite(byteCount);
        Cp1252.Instance.GetBytes(value, _buffer.Slice(_position, byteCount));
        _position += byteCount;
    }

    public void WriteBlob(ReadOnlySpan<byte> data)
    {
        WriteInt32(data.Length);
        EnsureCanWrite(data.Length);
        data.CopyTo(_buffer[_position..]);
        _position += data.Length;
    }
}
