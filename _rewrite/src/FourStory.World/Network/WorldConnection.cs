using FourStory.Protocol;
using FourStory.Protocol.Session;
using Microsoft.Extensions.Logging;

namespace FourStory.World.Network;

/// <summary>
/// One per accepted inter-server TCP connection on the World port. The C++
/// counterpart is <c>CTServer</c> (<c>Server/TWorldSvr/TServer.h</c>) — each
/// connected Map/Login/DM/Manager process is represented by one of these.
///
/// Same shape as <c>FourStory.Map.Network.MapConnection</c>; the duplication
/// between Map/Login/World will get folded into a shared
/// <c>TcpHostServer&lt;TConn&gt;</c> abstraction once we have three call sites
/// to generalise from.
/// </summary>
public sealed class WorldConnection : IAsyncDisposable
{
    private readonly ILogger<WorldConnection> _logger;
    private readonly WorldPacketDispatcher _dispatcher;
    public PacketSession Session { get; }
    public string RemoteAddress { get; }
    public WorldPeerState State { get; } = new();

    public WorldConnection(
        PacketSession session,
        string remoteAddress,
        WorldPacketDispatcher dispatcher,
        ILogger<WorldConnection> logger)
    {
        Session = session;
        RemoteAddress = remoteAddress;
        _dispatcher = dispatcher;
        _logger = logger;
    }

    public Task RunAsync(CancellationToken ct) => Session.RunAsync(OnPacketAsync, ct);

    private async ValueTask OnPacketAsync(MessageId id, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        try
        {
            await _dispatcher.DispatchAsync(this, id, body, ct).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex,
                "World handler exception for {Id} (0x{IdHex:X4}) from {Ip}",
                id, (ushort)id, RemoteAddress);
            throw;
        }
    }

    public ValueTask DisposeAsync() => Session.DisposeAsync();
}

/// <summary>
/// State for one connected SS peer. The C++ <c>CTServer</c> tracks
/// <c>m_wID</c> (server id, set on MW_CONNECT_ACK) plus the set of channels
/// the peer is serving (<c>m_mapCHANNEL</c>). We mirror just those two for
/// now — guild / party / corps caches live in module-level dictionaries on
/// the C++ side and will be hung off services rather than the connection.
/// </summary>
public sealed class WorldPeerState
{
    /// <summary>
    /// Per-process server ID claimed by this peer in MW_CONNECT_ACK. Null
    /// until the handshake completes. C++: <c>CTServer::m_wID</c>.
    /// </summary>
    public ushort? ServerId { get; set; }

    /// <summary>
    /// Channels this peer (a Map server) is hosting. C++:
    /// <c>CTServer::m_mapCHANNEL</c>. Populated from the channel list in
    /// MW_CONNECT_ACK.
    /// </summary>
    public HashSet<byte> Channels { get; } = new();

    public bool IsRegistered => ServerId is not null;
}

/// <summary>Routes decoded SS packets to their handler. Same pattern as Login's PacketDispatcher.</summary>
public sealed class WorldPacketDispatcher
{
    private readonly Dictionary<MessageId, Func<WorldConnection, ReadOnlyMemory<byte>, CancellationToken, ValueTask>> _handlers = new();
    private readonly ILogger<WorldPacketDispatcher> _logger;

    public WorldPacketDispatcher(ILogger<WorldPacketDispatcher> logger) => _logger = logger;

    public void Register(
        MessageId id,
        Func<WorldConnection, ReadOnlyMemory<byte>, CancellationToken, ValueTask> handler) =>
        _handlers[id] = handler;

    public ValueTask DispatchAsync(WorldConnection conn, MessageId id, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (_handlers.TryGetValue(id, out var h))
        {
            return h(conn, body, ct);
        }
        _logger.LogWarning(
            "Unhandled world packet {Id} (0x{IdHex:X4}) from {Ip}, payload {Bytes} bytes",
            id, (ushort)id, conn.RemoteAddress, body.Length);
        return ValueTask.CompletedTask;
    }
}
