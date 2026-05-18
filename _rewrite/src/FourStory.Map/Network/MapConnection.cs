using FourStory.Protocol;
using FourStory.Protocol.Session;
using Microsoft.Extensions.Logging;

namespace FourStory.Map.Network;

/// <summary>
/// One per accepted TCP connection on the map port. Same shape as
/// <c>FourStory.Login.ClientConnection</c> — handlers dispatch via the registered
/// <see cref="MapPacketDispatcher"/>.
///
/// TODO when we factor out a shared "TcpHostServer" abstraction this duplication
/// goes away; for now Map and Login each keep a copy because their session state
/// is application-specific.
/// </summary>
public sealed class MapConnection : IAsyncDisposable
{
    private readonly ILogger<MapConnection> _logger;
    private readonly MapPacketDispatcher _dispatcher;
    private readonly MapConnectionRegistry? _registry;
    private int? _registeredCharId;

    public PacketSession Session { get; }
    public string RemoteAddress { get; }
    /// <summary>The Map server's own IP as the client saw it (socket.LocalEndPoint).</summary>
    public string LocalAddress { get; }
    /// <summary>The Map server's listening port for this accepted connection.</summary>
    public ushort LocalPort { get; }
    public MapSessionState State { get; } = new();

    public MapConnection(
        PacketSession session,
        string remoteAddress,
        string localAddress,
        ushort localPort,
        MapPacketDispatcher dispatcher,
        ILogger<MapConnection> logger,
        MapConnectionRegistry? registry = null)
    {
        Session = session;
        RemoteAddress = remoteAddress;
        LocalAddress = localAddress;
        LocalPort = localPort;
        _dispatcher = dispatcher;
        _logger = logger;
        _registry = registry;
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
                "Map handler exception for {Id} (0x{IdHex:X4}) from {Ip}",
                id, (ushort)id, RemoteAddress);
            throw;
        }
    }

    /// <summary>
    /// Publishes this connection in the <see cref="MapConnectionRegistry"/> so
    /// inbound SS packets from World (which only carry <c>dwCharID</c>) can route
    /// back to it. Called from <c>ConnectHandler</c> once authentication has
    /// stamped <c>State.CharId</c>.
    /// </summary>
    public void AssociateCharacter(int charId)
    {
        _registeredCharId = charId;
        _registry?.Register(charId, this);
    }

    public ValueTask DisposeAsync()
    {
        if (_registry is not null && _registeredCharId is int c)
        {
            _registry.Unregister(c, this);
        }
        return Session.DisposeAsync();
    }
}

public sealed class MapSessionState
{
    public int? UserId { get; set; }
    public int? CharId { get; set; }
    public uint? Key { get; set; }
    public byte? Channel { get; set; }
    public bool IsAuthenticated => UserId is not null && CharId is not null;

    /// <summary>
    /// IP address the client claims as its Map endpoint (carried in CS_CONNECT_REQ,
    /// originally handed to the client by TLoginSvr in CS_START_ACK). Map relays
    /// this to World inside MW_ADDCHAR_ACK so World can associate the upcoming
    /// per-character session with the right Map peer. C++:
    /// <c>CTPlayer::m_dwIPAddr</c> (Server/TMapSvr/CSHandler.cpp:349/369).
    /// </summary>
    public uint? IpAddr { get; set; }

    /// <summary>
    /// Port pair to <see cref="IpAddr"/>. C++: <c>CTPlayer::m_wPort</c>.
    /// </summary>
    public ushort? Port { get; set; }

    /// <summary>
    /// True once the client has sent CS_CONREADY_REQ. The legacy server uses
    /// this signal (combined with character snapshot data delivered via SS
    /// from TWorldSvr) to trigger <c>InitMap</c>. See
    /// <c>Server/TMapSvr/CSHandler.cpp:402-415</c>.
    /// </summary>
    public bool IsReady { get; set; }

    /// <summary>
    /// True once World has delivered <c>MW_ENTERSVR_REQ</c> with the EF-loaded
    /// character snapshot and we've populated the per-character fields below.
    /// Combined with <see cref="IsReady"/> it gates the InitMap fan-out (the
    /// C++ <c>InitMap</c> at <c>TMapSvr.cpp:7909</c> requires both the
    /// CS_CONREADY signal AND the DM-loaded char data).
    /// </summary>
    public bool HasSnapshot { get; set; }

    /// <summary>
    /// Idempotency flag set when <c>MapInitOrchestrator.TryFireAsync</c> has
    /// successfully fanned out CS_ADDCONNECT_ACK + CS_CHARINFO_ACK.
    /// </summary>
    public bool InitMapDone { get; set; }

    // ---- Character snapshot ------------------------------------------------
    // Populated by EnterSvrHandler from MW_ENTERSVR_REQ (in C++ these fields
    // live on CTPlayer and are filled by OnDM_LOADCHAR_ACK at
    // Server/TMapSvr/SSHandler.cpp:4484-4629). We do not currently populate
    // every field the C++ player carries — only what InitMap / EnterMAP read
    // before fanning out the first wave of CS_* packets. Extend as we port
    // more of the gameplay surface.

    /// <summary>C++ <c>CTPlayer::m_strNAME</c>.</summary>
    public string? Name { get; set; }
    /// <summary>C++ <c>CTPlayer::m_bStartAct</c> — tutorial / new-character state.</summary>
    public byte? StartAct { get; set; }
    /// <summary>C++ <c>CTPlayer::m_bRealSex</c>.</summary>
    public byte? RealSex { get; set; }
    /// <summary>C++ <c>CTPlayer::m_bClass</c>.</summary>
    public byte? Class { get; set; }
    /// <summary>C++ <c>CTPlayer::m_bLevel</c>.</summary>
    public byte? Level { get; set; }
    /// <summary>C++ <c>CTPlayer::m_bRace</c>.</summary>
    public byte? Race { get; set; }
    /// <summary>C++ <c>CTPlayer::m_bCountry</c> — current kingdom alignment.</summary>
    public byte? Country { get; set; }
    /// <summary>C++ <c>CTPlayer::m_bOriCountry</c> — original kingdom (immutable).</summary>
    public byte? OriCountry { get; set; }
    /// <summary>C++ <c>CTPlayer::m_bSex</c> (cosmetic — can differ from RealSex via items).</summary>
    public byte? Sex { get; set; }
    public byte? Hair { get; set; }
    public byte? Face { get; set; }
    public byte? Body { get; set; }
    public byte? Pants { get; set; }
    public byte? Hand { get; set; }
    public byte? Foot { get; set; }
    public byte? HelmetHide { get; set; }
    public int? Gold { get; set; }
    public int? Silver { get; set; }
    public int? Cooper { get; set; }
    public int? Exp { get; set; }
    public int? HP { get; set; }
    public int? MP { get; set; }
    public short? SkillPoint { get; set; }
    public int? Region { get; set; }
    public byte? GuildLeave { get; set; }
    public int? GuildLeaveTime { get; set; }
    /// <summary>C++ <c>CTPlayer::m_wMapID</c> — first thing InitMap reads.</summary>
    public short? MapId { get; set; }
    public short? SpawnId { get; set; }
    public short? LastSpawnId { get; set; }
    public int? LastDestination { get; set; }
    public short? TemptedMon { get; set; }
    public byte? Aftermath { get; set; }
    /// <summary>C++ <c>CTPlayer::m_fPosX</c>.</summary>
    public float? PosX { get; set; }
    public float? PosY { get; set; }
    public float? PosZ { get; set; }
    public short? Dir { get; set; }
    public byte? StatLevel { get; set; }
    public byte? StatPoint { get; set; }
    public int? StatExp { get; set; }
    public int? RankPoint { get; set; }
}

/// <summary>Routes decoded packets to their handler. Same pattern as Login's PacketDispatcher.</summary>
public sealed class MapPacketDispatcher
{
    private readonly Dictionary<MessageId, Func<MapConnection, ReadOnlyMemory<byte>, CancellationToken, ValueTask>> _handlers = new();
    private readonly ILogger<MapPacketDispatcher> _logger;

    public MapPacketDispatcher(ILogger<MapPacketDispatcher> logger) => _logger = logger;

    public void Register(
        MessageId id,
        Func<MapConnection, ReadOnlyMemory<byte>, CancellationToken, ValueTask> handler) =>
        _handlers[id] = handler;

    public ValueTask DispatchAsync(MapConnection conn, MessageId id, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (_handlers.TryGetValue(id, out var h))
        {
            return h(conn, body, ct);
        }
        _logger.LogWarning(
            "Unhandled map packet {Id} (0x{IdHex:X4}) from {Ip}, payload {Bytes} bytes",
            id, (ushort)id, conn.RemoteAddress, body.Length);
        return ValueTask.CompletedTask;
    }
}
