using System.Buffers;
using System.Buffers.Binary;
using System.Threading.Channels;

namespace FourStory.Protocol.Session;

/// <summary>
/// One async read loop + one async write loop per TCP connection. Frames packets
/// using the legacy wire format (wSize-prefixed), drives <see cref="PacketCodec"/>
/// for encryption, and dispatches decoded packets to a callback.
///
/// Threading model: a single session has exactly one reader and one writer task
/// running concurrently. Both serialize their <see cref="PacketCodec"/> access
/// through their own direction (sendNumber / recvNumber are direction-private),
/// so the codec itself need not be thread-safe.
///
/// Replaces the C++ <c>CSession</c> IOCP plumbing (<c>Server/TNetLib/Session.cpp</c>).
/// </summary>
public sealed class PacketSession : IAsyncDisposable
{
    private readonly Stream _transport;
    private readonly bool _ownsTransport;
    private readonly PacketCodec _codec;
    private readonly Channel<OutboundPacket> _sendQueue;

    /// <summary>Per-packet entry on the write queue. The <see cref="WriteCompleted"/> source
    /// lets <see cref="SendAsync"/> return a Task that observes the actual wire write
    /// (not just the enqueue), which avoids races against codec state changes.</summary>
    private readonly record struct OutboundPacket(byte[] Packet, TaskCompletionSource WriteCompleted);

    private CancellationTokenSource? _cts;
    private Task? _readLoop;
    private Task? _writeLoop;
    private int _disposed;
    private long _lastActivityTicks = DateTimeOffset.UtcNow.UtcTicks;

    /// <summary>
    /// Wall-clock time of the most recent successful read or write. Used by the idle-session
    /// monitor to evict connections that have stopped exchanging traffic without closing the
    /// TCP socket (typical of NAT timeouts and half-open client crashes).
    /// </summary>
    public DateTimeOffset LastActivityUtc => new(Interlocked.Read(ref _lastActivityTicks), TimeSpan.Zero);

    /// <summary>Request the session to stop. Triggers cancellation of read and write loops.</summary>
    public void Stop() => _cts?.Cancel();

    public PacketSession(Stream transport, PacketCodec codec, bool ownsTransport = true)
    {
        ArgumentNullException.ThrowIfNull(transport);
        ArgumentNullException.ThrowIfNull(codec);
        _transport = transport;
        _ownsTransport = ownsTransport;
        _codec = codec;
        _sendQueue = Channel.CreateUnbounded<OutboundPacket>(new UnboundedChannelOptions
        {
            SingleReader = true,
            SingleWriter = false,
            AllowSynchronousContinuations = false,
        });
    }

    public PacketCodec Codec => _codec;

    /// <summary>
    /// Starts the read and write loops. Returns a task that completes when BOTH
    /// loops exit (e.g., transport closed, cancellation requested, or decode error).
    /// </summary>
    public Task RunAsync(PacketHandler onPacket, CancellationToken ct)
    {
        ArgumentNullException.ThrowIfNull(onPacket);
        ObjectDisposedException.ThrowIf(Volatile.Read(ref _disposed) != 0, this);
        if (_readLoop is not null)
        {
            throw new InvalidOperationException("Session already running.");
        }
        _cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        _readLoop = ReadLoopAsync(onPacket, _cts.Token);
        _writeLoop = WriteLoopAsync(_cts.Token);
        return Task.WhenAll(_readLoop, _writeLoop);
    }

    /// <summary>
    /// Enqueues an outbound packet AND returns a Task that completes only after the
    /// packet has been written to the underlying transport. Callers awaiting this Task
    /// can safely mutate codec state (e.g., toggle <see cref="PacketCodec.CryptEnabled"/>)
    /// for subsequent packets without racing against the in-flight write.
    /// </summary>
    public Task SendAsync(MessageId messageId, ReadOnlySpan<byte> body, CancellationToken ct = default)
    {
        return SendAsync((ushort)messageId, body, ct);
    }

    /// <inheritdoc cref="SendAsync(MessageId, ReadOnlySpan{byte}, CancellationToken)"/>
    public Task SendAsync(ushort messageId, ReadOnlySpan<byte> body, CancellationToken ct = default)
    {
        ObjectDisposedException.ThrowIf(Volatile.Read(ref _disposed) != 0, this);
        var totalSize = PacketHeader.Size + body.Length;
        if (totalSize > ProtocolConstants.MaxPacketSize)
        {
            throw new ArgumentException(
                $"Packet exceeds MAX_PACKET_SIZE: {totalSize} > {ProtocolConstants.MaxPacketSize}.", nameof(body));
        }

        var packet = new byte[totalSize];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)totalSize);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), messageId);
        // dwNumber (offset 4..7) and llChkSUM (offset 8..15) are filled in by codec.Encrypt.
        body.CopyTo(packet.AsSpan(PacketHeader.Size));

        var tcs = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
        if (!_sendQueue.Writer.TryWrite(new OutboundPacket(packet, tcs)))
        {
            tcs.SetException(new InvalidOperationException("Session is closing."));
        }
        return tcs.Task;
    }

    private async Task ReadLoopAsync(PacketHandler onPacket, CancellationToken ct)
    {
        var headerBuf = new byte[2];
        try
        {
            while (!ct.IsCancellationRequested)
            {
                // Step 1: read wSize (always plaintext on the wire).
                if (!await TryReadExactlyAsync(_transport, headerBuf, ct).ConfigureAwait(false))
                {
                    return; // peer closed mid-stream or before any byte
                }

                var wSize = BinaryPrimitives.ReadUInt16LittleEndian(headerBuf);
                if (wSize < PacketHeader.Size || wSize > ProtocolConstants.MaxPacketSize)
                {
                    throw new InvalidDataException($"Invalid wSize on wire: {wSize}.");
                }

                // Step 2: rent a buffer the size of the full packet, copy wSize, read body.
                var rented = ArrayPool<byte>.Shared.Rent(wSize);
                try
                {
                    headerBuf.CopyTo(rented, 0);
                    var remaining = rented.AsMemory(2, wSize - 2);
                    if (!await TryReadExactlyAsync(_transport, remaining, ct).ConfigureAwait(false))
                    {
                        throw new InvalidDataException(
                            $"Unexpected EOF: needed {wSize - 2} body bytes after header.");
                    }

                    // Step 3: decrypt in place.
                    var packetSpan = rented.AsSpan(0, wSize);
                    var result = _codec.TryDecrypt(packetSpan);
                    if (result != PacketDecryptResult.Ok)
                    {
                        throw new InvalidDataException($"Packet decryption failed: {result}.");
                    }

                    // Step 4: dispatch.
                    var messageId = BinaryPrimitives.ReadUInt16LittleEndian(packetSpan.Slice(2, 2));
                    var body = rented.AsMemory(PacketHeader.Size, wSize - PacketHeader.Size);
                    Interlocked.Exchange(ref _lastActivityTicks, DateTimeOffset.UtcNow.UtcTicks);
                    await onPacket((MessageId)messageId, body, ct).ConfigureAwait(false);
                }
                finally
                {
                    ArrayPool<byte>.Shared.Return(rented, clearArray: false);
                }
            }
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
            // expected on shutdown
        }
        finally
        {
            // Stopping the read loop also stops the write loop (peer disappeared / decode failed).
            _cts?.Cancel();
            _sendQueue.Writer.TryComplete();
        }
    }

    private async Task WriteLoopAsync(CancellationToken ct)
    {
        try
        {
            await foreach (var item in _sendQueue.Reader.ReadAllAsync(ct).ConfigureAwait(false))
            {
                try
                {
                    _codec.Encrypt(item.Packet);
                    await _transport.WriteAsync(item.Packet, ct).ConfigureAwait(false);
                    await _transport.FlushAsync(ct).ConfigureAwait(false);
                    Interlocked.Exchange(ref _lastActivityTicks, DateTimeOffset.UtcNow.UtcTicks);
                    item.WriteCompleted.TrySetResult();
                }
                catch (Exception ex)
                {
                    item.WriteCompleted.TrySetException(ex);
                    throw;
                }
            }
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
            // expected on shutdown
        }
        finally
        {
            _cts?.Cancel();
            // Drain any remaining queued packets so awaiters don't hang.
            while (_sendQueue.Reader.TryRead(out var leftover))
            {
                leftover.WriteCompleted.TrySetException(new OperationCanceledException("Session closed."));
            }
        }
    }

    private static async ValueTask<bool> TryReadExactlyAsync(Stream s, Memory<byte> buffer, CancellationToken ct)
    {
        var read = 0;
        while (read < buffer.Length)
        {
            var n = await s.ReadAsync(buffer[read..], ct).ConfigureAwait(false);
            if (n == 0)
            {
                return read == 0 ? false : throw new EndOfStreamException(
                    $"Unexpected EOF after {read}/{buffer.Length} bytes.");
            }
            read += n;
        }
        return true;
    }

    public async ValueTask DisposeAsync()
    {
        if (Interlocked.Exchange(ref _disposed, 1) != 0)
        {
            return;
        }
        _cts?.Cancel();
        _sendQueue.Writer.TryComplete();
        try
        {
            if (_readLoop is not null && _writeLoop is not null)
            {
                await Task.WhenAll(_readLoop, _writeLoop).ConfigureAwait(false);
            }
        }
        catch
        {
            // already-faulted loops are surfaced via RunAsync's returned Task; swallow here
        }
        _cts?.Dispose();
        if (_ownsTransport)
        {
            await _transport.DisposeAsync().ConfigureAwait(false);
        }
    }
}

/// <summary>
/// Handler invoked once per fully decoded inbound packet.
/// <paramref name="body"/> is rented from <c>ArrayPool</c>; do not capture it past
/// the awaited completion of the handler. Copy if you need to retain.
/// </summary>
public delegate ValueTask PacketHandler(MessageId messageId, ReadOnlyMemory<byte> body, CancellationToken ct);
