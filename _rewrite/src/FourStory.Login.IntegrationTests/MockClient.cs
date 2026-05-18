using System.Buffers.Binary;
using System.Collections.Concurrent;
using System.Net.Sockets;
using FourStory.Protocol;
using FourStory.Protocol.Crypto;
using FourStory.Protocol.Session;

namespace FourStory.Login.IntegrationTests;

/// <summary>
/// Test-only client that speaks the legacy wire protocol from the client perspective.
/// Mirrors what the original C++ client would do: build packets, XOR + RC4-encrypt
/// outbound, RC4 + XOR-decrypt inbound. Crypto can be toggled mid-session (matching
/// the post-login activation of <c>m_bUseCrypt</c>).
/// </summary>
internal sealed class MockClient : IAsyncDisposable
{
    private readonly TcpClient _tcp;
    private readonly NetworkStream _stream;
    private readonly Task _readLoop;
    private readonly CancellationTokenSource _cts;
    private uint _sendNumber;
    private uint _recvNumber;

    public bool CryptEnabled { get; set; }
    public ConcurrentQueue<(MessageId Id, byte[] Body)> Received { get; } = new();

    private MockClient(TcpClient tcp, NetworkStream stream)
    {
        _tcp = tcp;
        _stream = stream;
        _cts = new CancellationTokenSource();
        _readLoop = Task.Run(() => ReadLoopAsync(_cts.Token));
    }

    public static async Task<MockClient> ConnectAsync(string host, int port, CancellationToken ct)
    {
        var tcp = new TcpClient();
        await tcp.ConnectAsync(host, port, ct).ConfigureAwait(false);
        return new MockClient(tcp, tcp.GetStream());
    }

    /// <summary>
    /// Builds and sends a <see cref="MessageId.CS_LOGIN_REQ"/> using the legacy wire
    /// layout (CSHandler.cpp:148-164): wVersion, Zombie3, strPasswd, Zombie1, Zombie2,
    /// strUserID, dlCheck:INT64, llChecksum:INT64. Zombie strings are sent as length-0
    /// (the legacy client uses them as throwaway buffers).
    /// </summary>
    public async Task SendLoginRequestAsync(ushort version, string userId, string password, CancellationToken ct)
    {
        // Worst-case size: 2 + (4)+0 + (4+pw) + (4)+0 + (4)+0 + (4+id) + 8 + 8
        var buf = new byte[2 + 4 + 4 + 64 + 4 + 4 + 4 + 64 + 8 + 8];
        var w = new PacketWriter(buf);
        w.WriteUInt16(version);
        w.WriteString("");          // Zombie3
        w.WriteString(password);
        w.WriteString("");          // Zombie1
        w.WriteString("");          // Zombie2
        w.WriteString(userId);
        w.WriteInt64(0);            // dlCheck — exec-file integrity, server ignores when disabled
        w.WriteInt64(LoginChecksum.Compute(version));
        await SendAsync(MessageId.CS_LOGIN_REQ, w.WrittenSpan[..w.Position].ToArray(), ct).ConfigureAwait(false);
    }

    public async Task SendAsync(MessageId id, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        var totalSize = PacketHeader.Size + body.Length;
        var packet = new byte[totalSize];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)totalSize);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), (ushort)id);
        body.Span.CopyTo(packet.AsSpan(PacketHeader.Size));

        if (CryptEnabled)
        {
            _sendNumber++;
            BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(4, 4), _sendNumber);
            var key = KeyTable.KeyFor(_sendNumber);
            XorLayer.EncryptBody(packet, key);
            XorLayer.EncryptHeader(packet, key);
            Rc4Layer.TransformPacketPreservingWSize(packet);
        }

        await _stream.WriteAsync(packet, ct).ConfigureAwait(false);
        await _stream.FlushAsync(ct).ConfigureAwait(false);
    }

    public async Task<(MessageId Id, byte[] Body)> ReceiveAsync(MessageId expected, CancellationToken ct, TimeSpan? timeout = null)
    {
        var deadline = DateTime.UtcNow + (timeout ?? TimeSpan.FromSeconds(5));
        while (DateTime.UtcNow < deadline)
        {
            if (Received.TryDequeue(out var msg))
            {
                if (msg.Id != expected)
                {
                    throw new InvalidOperationException(
                        $"Expected {expected} (0x{(ushort)expected:X4}), got {msg.Id} (0x{(ushort)msg.Id:X4}). DebugLog:\n{DebugLog.Dump()}");
                }
                return msg;
            }
            await Task.Delay(15, ct).ConfigureAwait(false);
        }
        throw new TimeoutException($"No packet received within {timeout} (expected {expected}). DebugLog:\n{DebugLog.Dump()}");
    }

    private async Task ReadLoopAsync(CancellationToken ct)
    {
        var headerBuf = new byte[2];
        try
        {
            while (!ct.IsCancellationRequested)
            {
                if (!await ReadExactlyAsync(_stream, headerBuf, ct).ConfigureAwait(false))
                {
                    return;
                }
                var wSize = BinaryPrimitives.ReadUInt16LittleEndian(headerBuf);
                var packet = new byte[wSize];
                headerBuf.CopyTo(packet, 0);
                if (!await ReadExactlyAsync(_stream, packet.AsMemory(2), ct).ConfigureAwait(false))
                {
                    return;
                }

                if (CryptEnabled)
                {
                    // Server-to-client is XOR-only (no RC4 on inbound from client side).
                    _recvNumber++;
                    var key = KeyTable.KeyFor(_recvNumber);
                    XorLayer.DecryptHeader(packet, key);
                    if (!XorLayer.DecryptBody(packet, key))
                    {
                        throw new InvalidDataException("MockClient: server packet checksum failed.");
                    }
                }

                var id = (MessageId)BinaryPrimitives.ReadUInt16LittleEndian(packet.AsSpan(2, 2));
                var body = packet.AsSpan(PacketHeader.Size).ToArray();
                var dumpLen = (int)Math.Min(32u, wSize);
                DebugLog.Add($"recv id=0x{(ushort)id:X4} crypt={CryptEnabled} recvNum={_recvNumber} wSize={wSize} hex={Convert.ToHexString(packet.AsSpan(0, dumpLen))}");
                Received.Enqueue((id, body));
            }
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested) { }
        catch (IOException) { }
    }

    private static async ValueTask<bool> ReadExactlyAsync(Stream s, Memory<byte> buf, CancellationToken ct)
    {
        var read = 0;
        while (read < buf.Length)
        {
            var n = await s.ReadAsync(buf[read..], ct).ConfigureAwait(false);
            if (n == 0)
            {
                return read != 0 ? throw new EndOfStreamException() : false;
            }
            read += n;
        }
        return true;
    }

    public async ValueTask DisposeAsync()
    {
        await _cts.CancelAsync().ConfigureAwait(false);
        _stream.Close();
        _tcp.Close();
        try { await _readLoop.ConfigureAwait(false); } catch { /* ignore */ }
        _cts.Dispose();
    }
}
