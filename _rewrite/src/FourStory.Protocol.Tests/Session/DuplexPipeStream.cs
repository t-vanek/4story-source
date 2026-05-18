using System.IO.Pipelines;

namespace FourStory.Protocol.Tests.Session;

/// <summary>
/// Read+Write Stream backed by two <see cref="Pipe"/>s — a unit-test stand-in for a TCP
/// socket pair. Use <see cref="CreatePair"/> to get the two endpoints.
/// </summary>
internal sealed class DuplexPipeStream : Stream
{
    private readonly Stream _read;
    private readonly Stream _write;

    private DuplexPipeStream(Stream read, Stream write)
    {
        _read = read;
        _write = write;
    }

    public static (DuplexPipeStream a, DuplexPipeStream b) CreatePair()
    {
        var aToB = new Pipe();
        var bToA = new Pipe();
        var a = new DuplexPipeStream(bToA.Reader.AsStream(), aToB.Writer.AsStream());
        var b = new DuplexPipeStream(aToB.Reader.AsStream(), bToA.Writer.AsStream());
        return (a, b);
    }

    public override bool CanRead => true;
    public override bool CanWrite => true;
    public override bool CanSeek => false;
    public override long Length => throw new NotSupportedException();
    public override long Position { get => throw new NotSupportedException(); set => throw new NotSupportedException(); }
    public override void Flush() => _write.Flush();
    public override Task FlushAsync(CancellationToken ct) => _write.FlushAsync(ct);
    public override int Read(byte[] buffer, int offset, int count) => _read.Read(buffer, offset, count);
    public override ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken ct = default) => _read.ReadAsync(buffer, ct);
    public override void Write(byte[] buffer, int offset, int count) => _write.Write(buffer, offset, count);
    public override ValueTask WriteAsync(ReadOnlyMemory<byte> buffer, CancellationToken ct = default) => _write.WriteAsync(buffer, ct);
    public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
    public override void SetLength(long value) => throw new NotSupportedException();

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _read.Dispose();
            _write.Dispose();
        }
        base.Dispose(disposing);
    }
}
