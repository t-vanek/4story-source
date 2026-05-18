using System.Buffers.Binary;
using System.Collections.Concurrent;
using FourStory.Protocol;
using FourStory.Protocol.Crypto;
using FourStory.Protocol.Session;

namespace FourStory.Protocol.Tests.Session;

public class PacketSessionTests
{
    private static readonly TimeSpan TestTimeout = TimeSpan.FromSeconds(5);

    /// <summary>Builds a fully-encrypted client→server packet, as the legacy C++ client would.</summary>
    private static byte[] BuildClientWirePacket(uint sendNumber, MessageId messageId, ReadOnlySpan<byte> body)
    {
        var totalSize = PacketHeader.Size + body.Length;
        var packet = new byte[totalSize];
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(0, 2), (ushort)totalSize);
        BinaryPrimitives.WriteUInt16LittleEndian(packet.AsSpan(2, 2), (ushort)messageId);
        BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(4, 4), sendNumber);
        body.CopyTo(packet.AsSpan(PacketHeader.Size));

        var key = KeyTable.KeyFor(sendNumber);
        XorLayer.EncryptBody(packet, key);
        XorLayer.EncryptHeader(packet, key);
        Rc4Layer.TransformPacketPreservingWSize(packet);
        return packet;
    }

    [Fact]
    public async Task PlaintextSessions_SinglePacket_RoundTrip()
    {
        var (a, b) = DuplexPipeStream.CreatePair();
        await using var serverSession = new PacketSession(a, new PacketCodec(PeerType.Server));
        await using var clientSession = new PacketSession(b, new PacketCodec(PeerType.Server));

        var received = new TaskCompletionSource<(MessageId, byte[])>();
        using var cts = new CancellationTokenSource(TestTimeout);

        _ = serverSession.RunAsync((id, body, _) =>
        {
            received.TrySetResult((id, body.ToArray()));
            return ValueTask.CompletedTask;
        }, cts.Token);
        _ = clientSession.RunAsync((_, _, _) => ValueTask.CompletedTask, cts.Token);

        var bodyOut = new byte[] { 1, 2, 3, 4, 5 };
        await clientSession.SendAsync(MessageId.CS_LOGIN_REQ, bodyOut, cts.Token);

        var (id, bodyIn) = await received.Task.WaitAsync(cts.Token);
        Assert.Equal(MessageId.CS_LOGIN_REQ, id);
        Assert.Equal(bodyOut, bodyIn);
    }

    [Fact]
    public async Task PlaintextSessions_MultiPacketSequence_InOrder()
    {
        var (a, b) = DuplexPipeStream.CreatePair();
        await using var serverSession = new PacketSession(a, new PacketCodec(PeerType.Server));
        await using var clientSession = new PacketSession(b, new PacketCodec(PeerType.Server));

        var received = new ConcurrentQueue<int>();
        var doneTcs = new TaskCompletionSource();
        using var cts = new CancellationTokenSource(TestTimeout);

        const int packetCount = 25;

        _ = serverSession.RunAsync((_, body, _) =>
        {
            var n = BinaryPrimitives.ReadInt32LittleEndian(body.Span);
            received.Enqueue(n);
            if (received.Count == packetCount)
            {
                doneTcs.TrySetResult();
            }
            return ValueTask.CompletedTask;
        }, cts.Token);
        _ = clientSession.RunAsync((_, _, _) => ValueTask.CompletedTask, cts.Token);

        for (var i = 0; i < packetCount; i++)
        {
            var body = new byte[4];
            BinaryPrimitives.WriteInt32LittleEndian(body, i);
            await clientSession.SendAsync(MessageId.CS_LOGIN_REQ, body, cts.Token);
        }

        await doneTcs.Task.WaitAsync(cts.Token);
        Assert.Equal(Enumerable.Range(0, packetCount), received);
    }

    [Fact]
    public async Task ServerSession_DecryptsHandcraftedClientPackets()
    {
        // The server-side codec we're testing: peer is a Client, crypto enabled.
        // We don't use a "client" PacketSession because PacketCodec.Encrypt is one-sided
        // (server-out only — no RC4 layer). Instead we hand-craft wire-faithful client
        // packets and write them raw into the transport.
        var (server, peer) = DuplexPipeStream.CreatePair();
        await using var session = new PacketSession(
            server, new PacketCodec(PeerType.Client) { CryptEnabled = true });

        var received = new ConcurrentQueue<(MessageId, byte[])>();
        var doneTcs = new TaskCompletionSource();
        using var cts = new CancellationTokenSource(TestTimeout);

        const int packetCount = 7; // span the 7-key XOR table at least once
        _ = session.RunAsync((id, body, _) =>
        {
            received.Enqueue((id, body.ToArray()));
            if (received.Count == packetCount)
            {
                doneTcs.TrySetResult();
            }
            return ValueTask.CompletedTask;
        }, cts.Token);

        for (var i = 0; i < packetCount; i++)
        {
            var body = new byte[8];
            BinaryPrimitives.WriteInt32LittleEndian(body, i * 100);
            var wire = BuildClientWirePacket((uint)(i + 1), MessageId.CS_LOGIN_REQ, body);
            await peer.WriteAsync(wire, cts.Token);
        }
        await peer.FlushAsync(cts.Token);

        await doneTcs.Task.WaitAsync(cts.Token);

        var arr = received.ToArray();
        for (var i = 0; i < packetCount; i++)
        {
            Assert.Equal(MessageId.CS_LOGIN_REQ, arr[i].Item1);
            Assert.Equal(i * 100, BinaryPrimitives.ReadInt32LittleEndian(arr[i].Item2));
        }
    }

    [Fact]
    public async Task GarbageInput_FaultsRunTask()
    {
        var (server, peer) = DuplexPipeStream.CreatePair();
        await using var session = new PacketSession(
            server, new PacketCodec(PeerType.Client) { CryptEnabled = true });

        using var cts = new CancellationTokenSource(TestTimeout);
        var run = session.RunAsync((_, _, _) => ValueTask.CompletedTask, cts.Token);

        // Garbage: claims wSize=24, 22 bytes of body. Encryption will fail checksum.
        var garbage = new byte[24];
        BinaryPrimitives.WriteUInt16LittleEndian(garbage.AsSpan(0, 2), 24);
        for (var i = 2; i < garbage.Length; i++)
        {
            garbage[i] = 0xAB;
        }
        await peer.WriteAsync(garbage, cts.Token);
        await peer.FlushAsync(cts.Token);

        // Read loop should fault with InvalidDataException (sequence or checksum mismatch).
        var ex = await Assert.ThrowsAnyAsync<Exception>(() => run);
        Assert.True(
            ex is InvalidDataException ||
            ex.InnerException is InvalidDataException ||
            (ex is AggregateException agg && agg.InnerExceptions.Any(e => e is InvalidDataException)),
            $"Expected InvalidDataException, got {ex.GetType().Name}: {ex.Message}");
    }

    [Fact]
    public async Task SendAsync_OverMaxPacketSize_Throws()
    {
        var (a, _) = DuplexPipeStream.CreatePair();
        await using var session = new PacketSession(a, new PacketCodec(PeerType.Server));

        var oversized = new byte[ProtocolConstants.MaxPacketSize];
        await Assert.ThrowsAsync<ArgumentException>(async () =>
            await session.SendAsync(MessageId.CS_LOGIN_REQ, oversized));
    }

    [Fact]
    public async Task DisposeAsync_StopsLoops_Cleanly()
    {
        var (a, _) = DuplexPipeStream.CreatePair();
        var session = new PacketSession(a, new PacketCodec(PeerType.Server));
        using var cts = new CancellationTokenSource(TestTimeout);

        var run = session.RunAsync((_, _, _) => ValueTask.CompletedTask, cts.Token);
        await session.DisposeAsync();

        // RunAsync completes (possibly with cancellation) — must not hang.
        try { await run.WaitAsync(TimeSpan.FromSeconds(1)); }
        catch (OperationCanceledException) { /* expected */ }
    }
}
