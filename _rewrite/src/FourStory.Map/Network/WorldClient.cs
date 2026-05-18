using System.Net.Sockets;
using FourStory.Protocol;
using FourStory.Protocol.Session;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace FourStory.Map.Network;

/// <summary>
/// Outbound, long-lived SS connection from this Map process to the WorldSvr.
/// Mirrors the C++ <c>CTMapSvrModule::m_world</c> session — a single TCP socket
/// opened during init (<c>Server/TMapSvr/TMapSvr.cpp:949</c>) and used by every
/// <c>SendMW_*</c> in <c>SSSender.cpp</c> via <c>m_world.Say(pMSG)</c>.
///
/// Lifecycle:
/// <list type="bullet">
/// <item><description><b>Connect</b> with retry/backoff to <see cref="WorldEndpoint"/>.</description></item>
/// <item><description><b>Send</b> <c>MW_CONNECT_ACK</c> handshake immediately — same shape as
/// <c>CTMapSvrModule::SendMW_CONNECT_ACK</c> (<c>SSSender.cpp:210</c>):
/// <c>WORD wServerID = MAKEWORD(bServerID, SVRGRP_MAPSVR)</c>, then channel count + channels.</description></item>
/// <item><description><b>Run</b> the read loop. Inbound SS packets from World are dispatched through
/// <see cref="WorldClientDispatcher"/> (currently stub — gameplay subsystems will
/// register handlers as they land).</description></item>
/// <item><description><b>Reconnect</b> if the session ever drops: capped exponential backoff,
/// then start the cycle again. The original C++ code aborts the whole process
/// on World connect failure (<c>EC_INITSERVICE_CONNECTWORLD</c>); we prefer to
/// stay alive and retry so Map can recover without operator intervention.</description></item>
/// </list>
///
/// Concurrency: <see cref="SendAddCharAckAsync"/> is safe to call from any
/// handler. The session's send channel serializes writes, and callers awaiting
/// the returned task observe the actual wire write (see
/// <c>PacketSession.SendAsync</c>). While the session is reconnecting, callers
/// wait up to <see cref="SendReadyTimeout"/> for a fresh session; if that
/// elapses we throw, matching the legacy "drop the packet, log it" behaviour
/// of <c>m_world.Say</c> when the session is invalid.
/// </summary>
public sealed class WorldClient : BackgroundService
{
    /// <summary>Server-group identifier used in MW_CONNECT_ACK's <c>wServerID</c>
    /// high byte. C++: <c>SVRGRP_MAPSVR</c> = 4 (Lib/Own/TProtocol/include/CTProtocol.h).</summary>
    public const byte SvrGrpMapSvr = 4;

    private static readonly TimeSpan InitialBackoff = TimeSpan.FromSeconds(1);
    private static readonly TimeSpan MaxBackoff = TimeSpan.FromSeconds(30);

    /// <summary>How long a caller waits for the session to come up before
    /// failing. Short enough to keep CS_CONNECT_REQ responsive but long enough
    /// to ride out a single fast reconnect.</summary>
    public static readonly TimeSpan SendReadyTimeout = TimeSpan.FromSeconds(2);

    private readonly WorldEndpoint _endpoint;
    private readonly MapServerInfo _serverInfo;
    private readonly MapChannelList _channels;
    private readonly WorldClientDispatcher _dispatcher;
    private readonly ILogger<WorldClient> _logger;

    // _state is the single source of truth. Replaced atomically on connect /
    // disconnect. Readers grab a snapshot once and use it for the whole op.
    private sealed record State(PacketSession? Session, TaskCompletionSource ReadyTcs);
    private State _state;

    public WorldClient(
        WorldEndpoint endpoint,
        MapServerInfo serverInfo,
        MapChannelList channels,
        WorldClientDispatcher dispatcher,
        ILogger<WorldClient> logger)
    {
        _endpoint = endpoint;
        _serverInfo = serverInfo;
        _channels = channels;
        _dispatcher = dispatcher;
        _logger = logger;
        _state = new State(null, new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously));
    }

    /// <summary>True if a session is currently live. Useful for diagnostics /
    /// readiness probes; do not gate sends on this — there's a TOCTOU window.
    /// Just call <see cref="SendAddCharAckAsync"/> and handle the exception.</summary>
    public bool IsConnected => Volatile.Read(ref _state).Session is not null;

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation(
            "WorldClient starting: target={Host}:{Port} srvId={ServerId} channels=[{Channels}]",
            _endpoint.Host, _endpoint.Port, _serverInfo.ServerId,
            string.Join(",", _channels.Channels));

        var backoff = InitialBackoff;
        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                using var tcp = new TcpClient();
                await tcp.ConnectAsync(_endpoint.Host, _endpoint.Port, stoppingToken).ConfigureAwait(false);
                _logger.LogInformation("WorldClient connected to {Host}:{Port}", _endpoint.Host, _endpoint.Port);
                backoff = InitialBackoff;

                await using var stream = tcp.GetStream();
                // PeerType.Server: SS link, no RC4. Matches C++ `m_world.m_bUseCrypt = FALSE`
                // (TMapSvr.cpp:947) — both ends agree to skip the client crypt layer.
                var codec = new PacketCodec(PeerType.Server);
                await using var session = new PacketSession(stream, codec, ownsTransport: false);

                // Send MW_CONNECT_ACK BEFORE signalling ready — callers that race
                // in shouldn't see a session that hasn't done the handshake yet.
                // (We can call SendAsync against the session directly here without
                // going through SendAsyncInternal because no other path can have
                // the reference yet.)
                await SendConnectAckAsync(session, stoppingToken).ConfigureAwait(false);

                // Publish + signal ready in a single atomic swap. New waiters
                // arriving after this point see Session != null and skip the
                // TCS path entirely (WaitForSessionAsync's fast-return). Old
                // waiters parked on the previous state's TCS get woken below.
                var prevState = Interlocked.Exchange(
                    ref _state,
                    new State(session, new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously)));
                prevState.ReadyTcs.TrySetResult();

                await session.RunAsync(OnPacketAsync, stoppingToken).ConfigureAwait(false);

                _logger.LogInformation("WorldClient session ended cleanly");
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                return;
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex,
                    "WorldClient session error — reconnecting in {Backoff}s",
                    backoff.TotalSeconds);
            }
            finally
            {
                // Park a fresh "not ready" state. Any waiters who haven't observed
                // the previous completion yet will get the completed task they
                // captured; new waiters block on this fresh TCS.
                Interlocked.Exchange(ref _state, new State(null, new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously)));
            }

            if (stoppingToken.IsCancellationRequested)
            {
                return;
            }

            try
            {
                await Task.Delay(backoff, stoppingToken).ConfigureAwait(false);
            }
            catch (OperationCanceledException) { return; }

            backoff = TimeSpan.FromMilliseconds(
                Math.Min(backoff.TotalMilliseconds * 2, MaxBackoff.TotalMilliseconds));
        }
    }

    /// <summary>
    /// MW_CONNECT_ACK (0x9002). Layout matches
    /// <c>Server/TMapSvr/SSSender.cpp::SendMW_CONNECT_ACK</c> exactly:
    /// <code>
    /// WORD wServerID = MAKEWORD(m_bServerID, SVRGRP_MAPSVR);
    /// BYTE bCount = channels.size();
    /// for each ch: BYTE bChannel;
    /// </code>
    /// </summary>
    private async Task SendConnectAckAsync(PacketSession session, CancellationToken ct)
    {
        var payload = new byte[2 + 1 + _channels.Channels.Count];
        var writer = new PacketWriter(payload);
        // MAKEWORD(low, high) — low byte = server id, high byte = SVRGRP_MAPSVR.
        // little-endian on the wire: byte 0 = low, byte 1 = high.
        var wServerId = (ushort)(_serverInfo.ServerId | (SvrGrpMapSvr << 8));
        writer.WriteUInt16(wServerId);
        writer.WriteByte((byte)_channels.Channels.Count);
        foreach (var ch in _channels.Channels)
        {
            writer.WriteByte(ch);
        }
        await session.SendAsync(MessageId.MW_CONNECT_ACK, writer.WrittenSpan, ct).ConfigureAwait(false);
        _logger.LogInformation(
            "WorldClient sent MW_CONNECT_ACK srvId={ServerId} svrGrp={SvrGrp} channels={ChCount}",
            _serverInfo.ServerId, SvrGrpMapSvr, _channels.Channels.Count);
    }

    /// <summary>
    /// MW_ADDCHAR_ACK (0x9003). Map → World whenever a client successfully passes
    /// CS_CONNECT_REQ — World uses this to allocate/find the in-memory
    /// <c>TCHARACTER</c>, then drives the per-character bootstrap by sending
    /// <c>MW_ENTERSVR_REQ</c> back. C++:
    /// <c>Server/TMapSvr/SSSender.cpp::SendMW_ADDCHAR_ACK</c> →
    /// <c>Server/TWorldSvr/SSHandler.cpp::OnMW_ADDCHAR_ACK</c> (line 704).
    ///
    /// Wire layout (matches the C++ <c>operator&lt;&lt;</c> chain in order):
    /// <code>
    /// DWORD dwCharID;
    /// DWORD dwKEY;
    /// DWORD dwIPAddr;
    /// WORD  wPort;
    /// DWORD dwUserID;
    /// </code>
    /// </summary>
    public async Task SendAddCharAckAsync(
        uint charId,
        uint key,
        uint ipAddr,
        ushort port,
        uint userId,
        CancellationToken ct = default)
    {
        var session = await WaitForSessionAsync(ct).ConfigureAwait(false);

        var payload = new byte[4 + 4 + 4 + 2 + 4];
        var writer = new PacketWriter(payload);
        writer.WriteUInt32(charId);
        writer.WriteUInt32(key);
        writer.WriteUInt32(ipAddr);
        writer.WriteUInt16(port);
        writer.WriteUInt32(userId);

        await session.SendAsync(MessageId.MW_ADDCHAR_ACK, writer.WrittenSpan, ct).ConfigureAwait(false);
        _logger.LogInformation(
            "WorldClient sent MW_ADDCHAR_ACK char={CharId} user={UserId} key=0x{Key:X8} mapIp=0x{Ip:X8} port={Port}",
            charId, userId, key, ipAddr, port);
    }

    private async Task<PacketSession> WaitForSessionAsync(CancellationToken ct)
    {
        var snapshot = Volatile.Read(ref _state);
        if (snapshot.Session is not null)
        {
            return snapshot.Session;
        }

        using var timeoutCts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        timeoutCts.CancelAfter(SendReadyTimeout);
        try
        {
            await snapshot.ReadyTcs.Task.WaitAsync(timeoutCts.Token).ConfigureAwait(false);
        }
        catch (OperationCanceledException) when (!ct.IsCancellationRequested)
        {
            throw new InvalidOperationException(
                $"WorldClient: no live session after {SendReadyTimeout.TotalSeconds:F0}s wait.");
        }

        // After the ready signal, re-read state — the session field of the
        // *current* state should now be non-null.
        var live = Volatile.Read(ref _state);
        return live.Session ?? throw new InvalidOperationException(
            "WorldClient: ready signal fired but session went away again.");
    }

    private ValueTask OnPacketAsync(MessageId id, ReadOnlyMemory<byte> body, CancellationToken ct) =>
        _dispatcher.DispatchAsync(this, id, body, ct);
}

/// <summary>
/// Routes World→Map SS packets to their handlers. Symmetric to
/// <see cref="MapPacketDispatcher"/> but lives in the Map process and dispatches
/// the *client side* of the SS protocol (packets such as <c>MW_ENTERSVR_REQ</c>,
/// <c>MW_CHARDATA_REQ</c>, <c>MW_INVALIDCHAR_REQ</c>).
///
/// Empty out of the gate — gameplay subsystems will register handlers as they
/// land. Until then unknown packets just log a warning, mirroring the C++ Map
/// behaviour for unmapped SS opcodes.
/// </summary>
public sealed class WorldClientDispatcher
{
    private readonly Dictionary<MessageId, Func<WorldClient, ReadOnlyMemory<byte>, CancellationToken, ValueTask>> _handlers = new();
    private readonly ILogger<WorldClientDispatcher> _logger;

    public WorldClientDispatcher(ILogger<WorldClientDispatcher> logger) => _logger = logger;

    public void Register(
        MessageId id,
        Func<WorldClient, ReadOnlyMemory<byte>, CancellationToken, ValueTask> handler) =>
        _handlers[id] = handler;

    public ValueTask DispatchAsync(WorldClient client, MessageId id, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (_handlers.TryGetValue(id, out var h))
        {
            return h(client, body, ct);
        }
        _logger.LogWarning(
            "Unhandled World→Map SS packet {Id} (0x{IdHex:X4}), payload {Bytes} bytes",
            id, (ushort)id, body.Length);
        return ValueTask.CompletedTask;
    }
}
