using FourStory.Map.Network;
using FourStory.Protocol;
using Microsoft.Extensions.Logging;

namespace FourStory.Map.Handlers;

/// <summary>
/// Handles <c>CS_CONREADY_REQ</c> (0x5288), the empty-payload "I'm ready, send me
/// into the world" marker the client transmits after receiving a successful
/// <c>CS_CONNECT_ACK</c>.
///
/// Mirrors <c>Server/TMapSvr/CSHandler.cpp:402-415</c>:
/// <code>
/// DWORD CTMapSvrModule::OnCS_CONREADY_REQ(LPPACKETBUF pBUF)
/// {
///     CTPlayer *pPlayer = (CTPlayer *) pBUF->m_pSESSION;
///     if (!pPlayer->m_bExit)
///     {
///         if (!pPlayer->m_pMAP)
///             InitMap(pPlayer);                       // first entry
///         else if (pPlayer->m_bMain)
///             pPlayer->m_pMAP->EnterMAP(pPlayer, FALSE); // re-entry / channel hop
///     }
///     return EC_NOERROR;
/// }
/// </code>
///
/// The C++ branch picks between two flows:
/// <list type="bullet">
/// <item>
/// <description>
/// <b>First entry</b> (<c>m_pMAP == nullptr</c>): <c>InitMap</c> in
/// <c>TMapSvr.cpp:7909</c> resolves the player's <c>CTMap</c> (channel + party + mapId),
/// handles the castle / sky-garden / tournament special cases, repositions
/// companions, then calls <c>pTMAP->EnterMAP(pPlayer, TRUE)</c> which fans out
/// dozens of SC packets (CS_ADDCONNECT_ACK, CS_CHARINFO_ACK, CS_ENTER_ACK for
/// each neighbour, inventory, skills, quest state, ...).
/// </description>
/// </item>
/// <item>
/// <description>
/// <b>Re-entry</b> (<c>m_pMAP != nullptr &amp;&amp; m_bMain</c>): just
/// <c>EnterMAP(pPlayer, FALSE)</c> — re-announces visibility to neighbours
/// without re-initialising state.
/// </description>
/// </item>
/// </list>
///
/// <para><b>Current scope of this port.</b></para>
/// We accept the packet and validate session state, but do NOT execute
/// <c>InitMap</c> yet. The reason is data flow:
/// <c>pPlayer-&gt;m_wMapID</c>, <c>m_fPosX/Y/Z</c>, <c>m_bChannel</c>, etc. are
/// populated server-side by inter-server (SS) messages from TWorldSvr in
/// response to the <c>MW_ADDCHAR_ACK</c> the map sends right after
/// CS_CONNECT_REQ (see Server/TMapSvr/SSHandler.cpp:4653, 5714, 13266 for the
/// write-sites). Until the TWorldSvr port lands and SS plumbing exists in the
/// C# side, we have no character snapshot to feed <c>InitMap</c>. Sending a
/// half-baked CS_ENTER_ACK / CS_CHARINFO_ACK now would mislead the (eventual)
/// client — better to log a clean TODO and not send anything, which matches
/// what C++ does in the degenerate case (<c>InitMap</c> returns early when
/// <c>FindTMap</c> can't resolve a map and the player isn't main; see
/// TMapSvr.cpp:7922-7943).
/// </summary>
public sealed class ReadyHandler
{
    private readonly ILogger<ReadyHandler> _logger;

    public ReadyHandler(ILogger<ReadyHandler> logger)
    {
        _logger = logger;
    }

    public void Register(MapPacketDispatcher dispatcher)
    {
        dispatcher.Register(MessageId.CS_CONREADY_REQ, OnReadyAsync);
    }

    private ValueTask OnReadyAsync(MapConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        // CS_CONREADY_REQ has no payload — body should be empty.
        if (body.Length != 0)
        {
            _logger.LogWarning(
                "CS_CONREADY_REQ from {Ip} carried unexpected {Bytes}-byte payload (legacy wire format has none)",
                conn.RemoteAddress, body.Length);
        }

        // Guard analogous to C++ check `if (pPlayer->m_dwID == 0)` — the client
        // must have completed CS_CONNECT_REQ first. ConnectHandler stores
        // UserId/CharId on success; absence means an out-of-order packet.
        if (!conn.State.IsAuthenticated)
        {
            _logger.LogWarning(
                "CS_CONREADY_REQ from {Ip} before successful CS_CONNECT_REQ — ignoring (would close session in legacy)",
                conn.RemoteAddress);
            return ValueTask.CompletedTask;
        }

        // Flip the "ready for InitMap" flag. Downstream subsystems (SS handlers
        // from TWorldSvr, once ported) will key off this to know whether to
        // immediately call InitMap or wait for the readiness signal.
        // Legacy parallel: CTPlayer::m_bCheckedSession is set in OnCS_CONNECT_REQ
        // and `m_bExit` gates the InitMap call here. We use a single explicit
        // flag because the C++ negative-state encoding doesn't translate cleanly.
        conn.State.IsReady = true;

        // Symmetric to the IsReady check in EnterSvrHandler: InitMap requires
        // both the client's CONREADY signal AND the World-delivered char
        // snapshot. Whichever arrives second fires the trigger. We can hit
        // this branch when CS_CONREADY_REQ arrives after MW_ENTERSVR_REQ —
        // unusual but possible if World is fast and the client is slow.
        if (conn.State.HasSnapshot)
        {
            _logger.LogInformation(
                "CS_CONREADY_REQ char={CharId}: HasSnapshot already true — InitMap trigger fired (fan-out stubbed)",
                conn.State.CharId);
        }
        else
        {
            _logger.LogInformation(
                "CS_CONREADY_REQ user={UserId} char={CharId} ch={Channel} — flagged ready; waiting on MW_ENTERSVR_REQ snapshot before InitMap",
                conn.State.UserId, conn.State.CharId, conn.State.Channel);
        }

        // No response packet here — legacy `OnCS_CONREADY_REQ` returns
        // EC_NOERROR without sending anything synchronously. The flood of
        // SC packets only happens inside `InitMap` → `EnterMAP`, which we
        // intentionally do NOT call yet.
        //
        // TODO(map-init): port `CTMapSvrModule::InitMap` (TMapSvr.cpp:7909-8230)
        // once TWorldSvr is ported and `MS_*` SS handlers populate
        // MapSessionState.MapId / PosX/Y/Z / Country / SpawnId etc.
        // Then this handler should call something like
        //     await _mapInit.EnterAsync(conn, ct);
        // analogous to `if (!pPlayer->m_pMAP) InitMap(pPlayer)`.

        return ValueTask.CompletedTask;
    }
}
