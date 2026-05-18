using System.Runtime.InteropServices;

namespace FourStory.Protocol;

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketHeader
{
    public const int Size = 16;

    public ushort WSize;
    public ushort WId;
    public uint DwNumber;
    public long LlChkSum;

    public static bool IsHeaderComplete(int bytesAvailable) => bytesAvailable >= Size;

    public readonly bool IsPacketComplete(int bytesAvailable) => bytesAvailable >= WSize;
}
