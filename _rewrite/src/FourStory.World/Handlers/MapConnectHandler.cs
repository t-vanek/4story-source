using FourStory.Protocol;
using FourStory.World.Network;
using Microsoft.Extensions.Logging;

namespace FourStory.World.Handlers;

/// <summary>
/// Handles <c>MW_CONNECT_ACK</c> (0x9002) — the SS handshake packet a Map
/// server sends to the World server immediately after the TCP connection is
/// established. Despite the <c>_ACK</c> suffix this is the *first* packet
/// flowing from Map to World; the legacy naming convention reflects the
/// World process's role as the request-side ("World asked the cluster to
/// stand up, Map acknowledges it is online"), not the direction of the
/// initial TCP connect.
///
/// C++ reference: <c>Server/TWorldSvr/SSHandler.cpp:592-702</c>
/// (<c>CTWorldSvrModule::OnMW_CONNECT_ACK</c>).
///
/// Wire payload (read in C++ order):
/// <list type="number">
/// <item><description><c>WORD wServerID</c> — claimed server id of the connecting Map process.</description></item>
/// <item><description><c>BYTE bCount</c> — number of channels the Map hosts.</description></item>
/// <item><description><c>BYTE[bCount] bChannel</c> — the channel ids, one per byte.</description></item>
/// </list>
///
/// On success the C++ handler:
/// <list type="bullet">
/// <item><description>Rejects duplicate server ids (<c>EC_SESSION_DUPSERVERID</c>).</description></item>
/// <item><description>Inserts the peer into <c>m_mapSERVER</c>.</description></item>
/// <item><description>Sends back current battle-time state via
/// <c>SendMW_LOCALENABLE_REQ</c>, <c>SendMW_MISSIONENABLE_REQ</c>, optionally
/// <c>SendMW_SKYGARDENENABLE_REQ</c> (<c>SSSender.cpp:1926</c>).</description></item>
/// <item><description>Sends event / cash-item / castle / month-rank /
/// tournament state — a flood of bootstrap REQs to bring the new Map up to
/// the cluster's live state.</description></item>
/// </list>
///
/// <para><b>Current scope of this port.</b></para>
/// We parse the payload, dedupe by server id, populate
/// <see cref="WorldPeerState"/>, and log. The bootstrap-state fan-out is
/// deferred — none of that state exists in the C# side yet (no battle-time
/// scheduler, no in-memory event/cash-item caches, no rank service). Stubs
/// will land alongside the gameplay subsystems that own each piece of state.
/// </summary>
public sealed class MapConnectHandler
{
    private readonly ILogger<MapConnectHandler> _logger;

    public MapConnectHandler(ILogger<MapConnectHandler> logger)
    {
        _logger = logger;
    }

    public void Register(WorldPacketDispatcher dispatcher)
    {
        dispatcher.Register(MessageId.MW_CONNECT_ACK, OnConnectAsync);
    }

    private ValueTask OnConnectAsync(WorldConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        var r = new PacketReader(body.Span);
        var serverId = r.ReadUInt16();
        var channelCount = r.ReadByte();

        var channels = new List<byte>(channelCount);
        for (var i = 0; i < channelCount; i++)
        {
            channels.Add(r.ReadByte());
        }

        // Mirrors the C++ `m_mapSERVER.find(wServerID)` check
        // (SSHandler.cpp:602-604) — a Map process is uniquely keyed by its
        // configured server id; reconnects from a stale peer with the same id
        // are rejected. For now we just log; a follow-up will close the
        // session (the legacy code returns EC_SESSION_DUPSERVERID which
        // tears the session down).
        if (conn.State.IsRegistered)
        {
            _logger.LogWarning(
                "MW_CONNECT_ACK from {Ip} on an already-registered session (existing srvId={Existing}, new srvId={New}) — ignoring",
                conn.RemoteAddress, conn.State.ServerId, serverId);
            return ValueTask.CompletedTask;
        }

        conn.State.ServerId = serverId;
        foreach (var ch in channels)
        {
            conn.State.Channels.Add(ch);
        }

        _logger.LogInformation(
            "MW_CONNECT_ACK from {Ip}: srvId={ServerId} channels=[{Channels}] — registered",
            conn.RemoteAddress, serverId, string.Join(",", channels));

        // TODO(world-bootstrap): port the C++ fan-out
        // (SSHandler.cpp:619-700) once the matching subsystems exist:
        //   - SendMW_LOCALENABLE_REQ      — needs BattleTimeService
        //   - SendMW_MISSIONENABLE_REQ    — needs BattleTimeService
        //   - SendMW_SKYGARDENENABLE_REQ  — needs BattleTimeService (skygarden build)
        //   - SendMW_EVENTUPDATE_REQ      — needs EventService (m_mapEVENT)
        //   - SendMW_CASHITEMSALE_REQ     — needs CashItemSaleService
        //   - SendMW_CASTLEAPPLICANTCOUNT_REQ — needs GuildService.GetCastleApplicantCount
        //   - SendMW_MONTHRANKLIST_REQ    — needs RankService (m_arMonthRank)
        //   - TournamentInfo(pSERVER)     — needs TournamentService
        // Until these exist, a real Map peer that connects will sit in the
        // "registered but uninitialised" state — which is harmless for the
        // CS_CONREADY_REQ → InitMap flow because the snapshot data (MW_ENTERSVR_REQ
        // etc.) is computed per-character on demand, not at handshake time.

        return ValueTask.CompletedTask;
    }
}
