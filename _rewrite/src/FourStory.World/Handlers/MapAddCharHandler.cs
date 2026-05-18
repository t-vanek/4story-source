using FourStory.Persistence;
using FourStory.Persistence.Game;
using FourStory.Protocol;
using FourStory.World.Network;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;

namespace FourStory.World.Handlers;

/// <summary>
/// Handles <c>MW_ADDCHAR_ACK</c> (0x9003) — sent by a Map peer to the World
/// process immediately after a client passes its CS_CONNECT_REQ. World uses
/// this to allocate (or find an existing) <c>TCHARACTER</c> entry for the
/// connecting character, load the persistent character row from the DB, and
/// hand the snapshot back to Map via <c>MW_ENTERSVR_REQ</c>.
///
/// C++ reference: <c>Server/TWorldSvr/SSHandler.cpp:704-845</c>
/// (<c>CTWorldSvrModule::OnMW_ADDCHAR_ACK</c>).
///
/// Wire payload (read in C++ order):
/// <list type="number">
/// <item><description><c>DWORD dwCharID</c></description></item>
/// <item><description><c>DWORD dwKEY</c></description></item>
/// <item><description><c>DWORD dwIPAddr</c></description></item>
/// <item><description><c>WORD wPort</c></description></item>
/// <item><description><c>DWORD dwUserID</c></description></item>
/// </list>
///
/// <para><b>Architectural shortcut vs. C++.</b></para>
/// The legacy server bounces the per-character load through a separate
/// DataMaster (DM) process: World only sends the simple
/// <c>{bDBLoad=TRUE, charId, key}</c> form of <c>MW_ENTERSVR_REQ</c>, Map then
/// asks DM for <c>DM_ENTERMAPSVR_REQ</c> + <c>DM_LOADCHAR_REQ</c>, and DM does
/// the <c>CSPLoadCharacter</c> stored proc and replies with
/// <c>DM_LOADCHAR_ACK</c> carrying the full snapshot
/// (Server/TMapSvr/SSHandler.cpp:4424-4629).
///
/// The C# rewrite has no DM peer: World owns EF access to <c>GameDbContext</c>
/// directly, so we collapse the chain. We load <c>TCHARTABLE</c> on this side
/// and send the snapshot inline inside <c>MW_ENTERSVR_REQ</c>. Wire format is
/// <i>not</i> 1:1 with the legacy MW_ENTERSVR_REQ in this case — see
/// <see cref="EnterSvrPacket"/> for the layout we use, and the matching
/// <c>FourStory.Map.Handlers.EnterSvrHandler</c> for the receive side.
/// </summary>
public sealed class MapAddCharHandler
{
    private readonly IDbContextFactory<GameDbContext> _gameFactory;
    private readonly ILogger<MapAddCharHandler> _logger;

    public MapAddCharHandler(
        IDbContextFactory<GameDbContext> gameFactory,
        ILogger<MapAddCharHandler> logger)
    {
        _gameFactory = gameFactory;
        _logger = logger;
    }

    public void Register(WorldPacketDispatcher dispatcher)
    {
        dispatcher.Register(MessageId.MW_ADDCHAR_ACK, OnAddCharAsync);
    }

    private async ValueTask OnAddCharAsync(WorldConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        var r = new PacketReader(body.Span);
        var charId = r.ReadUInt32();
        var key = r.ReadUInt32();
        var ipAddr = r.ReadUInt32();
        var port = r.ReadUInt16();
        var userId = r.ReadUInt32();

        if (!conn.State.IsRegistered)
        {
            // Mirrors C++ guard implicit in OnMW_ADDCHAR_ACK: the handler casts
            // pBUF->m_pSESSION to CTServer and accesses pSERVER->m_wID, which
            // is zero/garbage if MW_CONNECT_ACK never ran. The legacy code
            // doesn't explicitly check; we do because logging an "ignored"
            // packet is friendlier than silently associating a character with
            // server id 0.
            _logger.LogWarning(
                "MW_ADDCHAR_ACK from {Ip} before MW_CONNECT_ACK — ignoring (char={CharId} user={UserId})",
                conn.RemoteAddress, charId, userId);
            return;
        }

        _logger.LogInformation(
            "MW_ADDCHAR_ACK from srvId={ServerId} char={CharId} user={UserId} key=0x{Key:X8} mapIp=0x{Ip:X8} port={Port}",
            conn.State.ServerId, charId, userId, key, ipAddr, port);

        // -------------------------------------------------------------------
        // Load the persistent character row. In C++ this is CSPLoadCharacter
        // executed by DM (Server/TWorldSvr/DBAccess.h::CSPLoadCharacter — DM
        // owns the stored proc, sends back via DM_LOADCHAR_ACK). We load
        // TCHARTABLE directly. If the row is missing this means the client is
        // pointing at a non-existent / deleted character id — legacy behavior
        // is to send DM_LOADCHAR_ACK with bResult != 0 which Map then relays
        // as MW_ENTERSVR_ACK with bResult=CN_INTERNAL (CSHandler.cpp:4416 path
        // "Load Char Error! CharID: %d"). We can't easily replicate that
        // chain without a full negative-ack packet; for now we log and drop —
        // the client will time out waiting for CS_CHARINFO_ACK and disconnect.
        //
        // TODO(world-error-ack): once we port the negative-result paths of
        // MW_ENTERSVR_ACK (CN_ALREADYEXIST / CN_INTERNAL), send a proper
        // failure rather than silently dropping. C++ ref:
        // Server/TMapSvr/SSHandler.cpp:3138-3145 (DM_ENTERMAPSVR_ACK error).
        // -------------------------------------------------------------------

        await using var db = await _gameFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        var ch = await db.TCHARTABLEs
            .AsNoTracking()
            .FirstOrDefaultAsync(t => t.dwCharID == (int)charId, ct)
            .ConfigureAwait(false);

        if (ch is null)
        {
            _logger.LogWarning(
                "MW_ADDCHAR_ACK char={CharId}: TCHARTABLE row missing — replying MW_INVALIDCHAR_REQ so Map kicks the session",
                charId);
            await SendInvalidCharAsync(conn, charId, key, ct).ConfigureAwait(false);
            return;
        }
        if (ch.bDelete != 0)
        {
            _logger.LogWarning(
                "MW_ADDCHAR_ACK char={CharId}: TCHARTABLE.bDelete={Del} — soft-deleted, replying MW_INVALIDCHAR_REQ",
                charId, ch.bDelete);
            await SendInvalidCharAsync(conn, charId, key, ct).ConfigureAwait(false);
            return;
        }

        // TODO(world-char-cache): port SSHandler.cpp:727-845 in-memory
        // TCHARACTER / TCHARCON maps. We currently re-load from DB on every
        // MW_ADDCHAR_ACK; the legacy server keeps a per-process LRU keyed by
        // charId to handle channel hops without DB hits. Not needed for the
        // happy path of a single connect — defer until we wire up channel
        // switching (MW_CHGCHANNEL_REQ).
        //
        // TODO(world-reentry): port the else-branch at SSHandler.cpp:803-845.
        // On a second MW_ADDCHAR_ACK for the same charId, validate the
        // (key, ip, port) tuple against the cached TCHARCON; mismatch ->
        // MW_INVALIDCHAR_REQ + CloseChar.
        //
        // TODO(world-party-guild): the legacy load chain also rehydrates
        // party (m_pParty), guild (m_pGuild), tactics (m_pTactics) bindings
        // from the in-memory dictionaries on World — see
        // SSHandler.cpp::OnMW_CHARDATA_REQ. Skipped here until we port those
        // subsystems.

        var payload = EnterSvrPacket.Build(charId, key, ch);
        await conn.Session.SendAsync(MessageId.MW_ENTERSVR_REQ, payload, ct).ConfigureAwait(false);

        _logger.LogInformation(
            "MW_ENTERSVR_REQ -> srvId={ServerId} char={CharId} name='{Name}' map={MapId} lvl={Level} pos=({X:F2},{Y:F2},{Z:F2}) country={Country}",
            conn.State.ServerId, charId, ch.szNAME, ch.wMapID, ch.bLevel, ch.fPosX, ch.fPosY, ch.fPosZ, ch.bCountry);
    }

    /// <summary>
    /// Tells the Map peer to drop the named char's session. Mirrors
    /// <c>CTWorldSvrModule::OnMW_ADDCHAR_ACK</c> calling
    /// <c>SendMW_INVALIDCHAR_REQ(dwCharID, dwKEY)</c> on the duplicate / corrupt-key
    /// branches at Server/TWorldSvr/SSHandler.cpp:820.
    /// </summary>
    private static async Task SendInvalidCharAsync(WorldConnection conn, uint charId, uint key, CancellationToken ct)
    {
        Span<byte> buf = stackalloc byte[8];
        var w = new PacketWriter(buf);
        w.WriteUInt32(charId);
        w.WriteUInt32(key);
        await conn.Session.SendAsync(MessageId.MW_INVALIDCHAR_REQ, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);
    }
}

/// <summary>
/// Builds the C#-extended <c>MW_ENTERSVR_REQ</c> payload. The leading
/// <c>{bDBLoad, charId, key}</c> tuple matches the legacy "simple form"
/// (<c>Server/TWorldSvr/SSSender.cpp::SendMW_ENTERSVR_REQ</c> line 164). The
/// remainder is a flattening of the per-character fields that DM_LOADCHAR_ACK
/// carries in the C++ flow (<c>Server/TMapSvr/SSHandler.cpp:4484-4629</c>),
/// minus the bits that depend on subsystems not yet ported (TSECURE security
/// code, PCBANG state, post-counts, secure-currency unlock).
///
/// <para><b>Layout</b> (little-endian, lengths in bytes):</para>
/// <code>
/// BYTE   bDBLoad        // always 0 — "data inline", not "ask DM"
/// DWORD  dwCharID
/// DWORD  dwKEY
/// string szNAME         // int32 length + CP1252 bytes
/// BYTE   bStartAct
/// BYTE   bRealSex
/// BYTE   bClass
/// BYTE   bLevel
/// BYTE   bRace
/// BYTE   bCountry
/// BYTE   bOriCountry
/// BYTE   bSex
/// BYTE   bHair
/// BYTE   bFace
/// BYTE   bBody
/// BYTE   bPants
/// BYTE   bHand
/// BYTE   bFoot
/// BYTE   bHelmetHide
/// INT32  dwGold
/// INT32  dwSilver
/// INT32  dwCooper
/// INT32  dwEXP
/// INT32  dwHP
/// INT32  dwMP
/// INT16  wSkillPoint
/// INT32  dwRegion
/// BYTE   bGuildLeave
/// INT32  dwGuildLeaveTime
/// INT16  wMapID
/// INT16  wSpawnID
/// INT16  wLastSpawnID
/// INT32  dwLastDestination
/// INT16  wTemptedMon
/// BYTE   bAftermath
/// FLOAT  fPosX
/// FLOAT  fPosY
/// FLOAT  fPosZ
/// INT16  wDIR
/// BYTE   bStatLevel
/// BYTE   bStatPoint
/// INT32  dwStatExp
/// INT32  dwRankPoint
/// </code>
/// </summary>
internal static class EnterSvrPacket
{
    public static byte[] Build(uint charId, uint key, TCHARTABLE ch)
    {
        // Worst-case payload size. The name is the only variable-length piece —
        // bounded by NAME_LENGTH (32) in the C++ schema, but CP1252 is single-byte
        // so length-in-chars == length-in-bytes. We pad generously to cover any
        // future field additions without revisiting this allocation.
        // Fixed-field tally: 9 (hdr) + 15 (bytes) + 24 (6×int32) + 2 + 4 + 1 + 4 +
        // 6 (3×int16) + 4 + 2 + 1 + 12 (3×float) + 2 + 2 + 8 = ~94 bytes.
        var nameByteEstimate = 4 + (ch.szNAME?.Length ?? 32) * 2;  // CP1252 safe upper bound
        var buf = new byte[nameByteEstimate + 256];
        var w = new PacketWriter(buf);

        w.WriteByte(0);              // bDBLoad = 0 (data inline)
        w.WriteUInt32(charId);
        w.WriteUInt32(key);

        w.WriteString(ch.szNAME ?? string.Empty);
        w.WriteByte(ch.bStartAct);
        w.WriteByte(ch.bRealSex);
        w.WriteByte(ch.bClass);
        w.WriteByte(ch.bLevel);
        w.WriteByte(ch.bRace);
        w.WriteByte(ch.bCountry);
        w.WriteByte(ch.bOriCountry);
        w.WriteByte(ch.bSex);
        w.WriteByte(ch.bHair);
        w.WriteByte(ch.bFace);
        w.WriteByte(ch.bBody);
        w.WriteByte(ch.bPants);
        w.WriteByte(ch.bHand);
        w.WriteByte(ch.bFoot);
        w.WriteByte(ch.bHelmetHide);
        w.WriteInt32(ch.dwGold);
        w.WriteInt32(ch.dwSilver);
        w.WriteInt32(ch.dwCooper);
        w.WriteInt32(ch.dwEXP);
        w.WriteInt32(ch.dwHP);
        w.WriteInt32(ch.dwMP);
        w.WriteInt16(ch.wSkillPoint);
        w.WriteInt32(ch.dwRegion);
        w.WriteByte(ch.bGuildLeave);
        w.WriteInt32(ch.dwGuildLeaveTime);
        w.WriteInt16(ch.wMapID);
        w.WriteInt16(ch.wSpawnID);
        w.WriteInt16(ch.wLastSpawnID);
        w.WriteInt32(ch.dwLastDestination);
        w.WriteInt16(ch.wTemptedMon);
        w.WriteByte(ch.bAftermath);
        w.WriteSingle(ch.fPosX);
        w.WriteSingle(ch.fPosY);
        w.WriteSingle(ch.fPosZ);
        w.WriteInt16(ch.wDIR);
        w.WriteByte(ch.bStatLevel ?? 0);
        w.WriteByte(ch.bStatPoint ?? 0);
        w.WriteInt32(ch.dwStatExp ?? 0);
        w.WriteInt32(ch.dwRankPoint);

        return w.WrittenSpan.ToArray();
    }
}
