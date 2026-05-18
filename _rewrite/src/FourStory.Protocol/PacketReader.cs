using System.Buffers.Binary;
using System.Runtime.CompilerServices;
using FourStory.Protocol.Encoding;

namespace FourStory.Protocol;

/// <summary>
/// Reads primitives from a packet payload in legacy little-endian wire format.
/// Mirrors the C++ <c>CPacket::operator&gt;&gt;</c> chain.
/// </summary>
public ref struct PacketReader
{
    private readonly ReadOnlySpan<byte> _buffer;
    private int _position;

    public PacketReader(ReadOnlySpan<byte> buffer)
    {
        _buffer = buffer;
        _position = 0;
    }

    public readonly int Position => _position;
    public readonly int Length => _buffer.Length;
    public readonly int Remaining => _buffer.Length - _position;
    public readonly bool IsEof => _position >= _buffer.Length;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    private void EnsureCanRead(int count)
    {
        if (_position + count > _buffer.Length)
        {
            throw new InvalidOperationException(
                $"Packet underflow: tried to read {count} bytes at position {_position}, buffer length {_buffer.Length}.");
        }
    }

    public byte ReadByte()
    {
        EnsureCanRead(1);
        return _buffer[_position++];
    }

    public sbyte ReadSByte() => unchecked((sbyte)ReadByte());

    public ushort ReadUInt16()
    {
        EnsureCanRead(2);
        var v = BinaryPrimitives.ReadUInt16LittleEndian(_buffer.Slice(_position, 2));
        _position += 2;
        return v;
    }

    public short ReadInt16()
    {
        EnsureCanRead(2);
        var v = BinaryPrimitives.ReadInt16LittleEndian(_buffer.Slice(_position, 2));
        _position += 2;
        return v;
    }

    public uint ReadUInt32()
    {
        EnsureCanRead(4);
        var v = BinaryPrimitives.ReadUInt32LittleEndian(_buffer.Slice(_position, 4));
        _position += 4;
        return v;
    }

    /// <summary>Reads a 32-bit signed integer. Maps to MSVC <c>int</c>/<c>long</c> on the wire (both 32-bit).</summary>
    public int ReadInt32()
    {
        EnsureCanRead(4);
        var v = BinaryPrimitives.ReadInt32LittleEndian(_buffer.Slice(_position, 4));
        _position += 4;
        return v;
    }

    public long ReadInt64()
    {
        EnsureCanRead(8);
        var v = BinaryPrimitives.ReadInt64LittleEndian(_buffer.Slice(_position, 8));
        _position += 8;
        return v;
    }

    public float ReadSingle()
    {
        EnsureCanRead(4);
        var v = BinaryPrimitives.ReadSingleLittleEndian(_buffer.Slice(_position, 4));
        _position += 4;
        return v;
    }

    /// <summary>
    /// Reads a length-prefixed string: <c>int32 length</c> then <c>length</c> bytes of CP1252 (Windows-1252).
    /// </summary>
    public string ReadString()
    {
        var length = ReadInt32();
        if (length == 0)
        {
            return string.Empty;
        }
        if (length < 0)
        {
            throw new InvalidOperationException($"Negative string length on wire: {length}.");
        }
        EnsureCanRead(length);
        var slice = _buffer.Slice(_position, length);
        _position += length;
        return Cp1252.Instance.GetString(slice);
    }

    /// <summary>Reads a length-prefixed binary blob (<c>int32 length</c> + raw bytes).</summary>
    public byte[] ReadBlob()
    {
        var length = ReadInt32();
        if (length <= 0)
        {
            return [];
        }
        EnsureCanRead(length);
        var result = _buffer.Slice(_position, length).ToArray();
        _position += length;
        return result;
    }
}
