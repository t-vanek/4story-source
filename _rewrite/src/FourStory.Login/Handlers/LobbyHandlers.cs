using System.Net;
using FourStory.Login.Auth;
using FourStory.Login.Services;
using FourStory.Persistence;
using FourStory.Protocol;
using FourStory.Protocol.Encoding;
using FourStory.Shared;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Handlers;

/// <summary>
/// Post-login lobby flow handlers. Mirrors <c>Server/TLoginSvr/CSHandler.cpp</c>
/// methods <c>OnCS_GROUPLIST_REQ</c>, <c>OnCS_CHANNELLIST_REQ</c>,
/// <c>OnCS_CHARLIST_REQ</c>, <c>OnCS_START_REQ</c>.
/// </summary>
public sealed class LobbyHandlers
{
    /// <summary>BOW_SERVER_ID from <c>NetCode.h:143</c> — Battle of Worlds shard server ID.</summary>
    // private const byte BowServerId = 30;
    /// <summary>BR_SERVER_ID from <c>NetCode.h:146</c> — Battle Royale shard server ID.</summary>
    private const byte BrServerId = 50;

    /// <summary>STORAGE_INVEN from <c>Lib/Own/TProtocol/include/NetCode.h:2243</c>.</summary>
    private const byte StorageInven = 0;
    /// <summary>INVEN_EQUIP from <c>NetCode.h:47</c> — equipped-items inventory slot.</summary>
    private const int InvenEquip = 0xFE;
    /// <summary>TOWNER_CHAR from <c>NetCode.h:2250</c> — item owner is a player character.</summary>
    private const byte TownerChar = 0;

    /// <summary>
    /// Per-ACK prelude byte. Legacy <c>GetCheckFilePoint(pUser)</c> returns a non-zero
    /// value only when the exec-file integrity feature is active; we don't ship that, so
    /// it's always 0. Sent at the start of every CS_*_ACK that legacy prefixed with it.
    /// </summary>
    private const byte CheckFilePoint = 0;

    private readonly IDbContextFactory<GlobalDbContext> _dbFactory;
    private readonly IDbContextFactory<GameDbContext> _gameDbFactory;
    private readonly CharService _charService;
    private readonly MapServerLocator _mapLocator;
    private readonly ILogger<LobbyHandlers> _logger;

    public LobbyHandlers(
        IDbContextFactory<GlobalDbContext> dbFactory,
        IDbContextFactory<GameDbContext> gameDbFactory,
        CharService charService,
        MapServerLocator mapLocator,
        ILogger<LobbyHandlers> logger)
    {
        _dbFactory = dbFactory;
        _gameDbFactory = gameDbFactory;
        _charService = charService;
        _mapLocator = mapLocator;
        _logger = logger;
    }

    public void Register(PacketDispatcher dispatcher)
    {
        dispatcher.Register(MessageId.CS_GROUPLIST_REQ, OnGroupListAsync);
        dispatcher.Register(MessageId.CS_CHANNELLIST_REQ, OnChannelListAsync);
        dispatcher.Register(MessageId.CS_CHARLIST_REQ, OnCharListAsync);
        dispatcher.Register(MessageId.CS_CREATECHAR_REQ, OnCreateCharAsync);
        dispatcher.Register(MessageId.CS_DELCHAR_REQ, OnDeleteCharAsync);
        dispatcher.Register(MessageId.CS_START_REQ, OnStartAsync);
        dispatcher.Register(MessageId.CS_AGREEMENT_REQ, OnAgreementAsync);
        dispatcher.Register(MessageId.CS_HOTSEND_REQ, OnHotsendAsync);
        dispatcher.Register(MessageId.CS_VETERAN_REQ, OnVeteranAsync);
        dispatcher.Register(MessageId.CS_SECURITYCONFIRM_ACK, OnSecurityConfirmAckAsync);
    }

    // ===== CS_GROUPLIST_REQ → CS_GROUPLIST_ACK (CSHandler.cpp:492-551) =====
    // Wire layout: BYTE bCount, BYTE CheckFilePoint, then repeated
    //   STRING szNAME, BYTE bGroupID, BYTE bType, BYTE bStatus, BYTE bCharCount
    // bStatus is the WIRE status (TSTATUS_SLEEP/NORMAL/BUSY/FULL — NetCode.h:939) derived
    // from the DB TSVR_STATUS + the current-user population vs wFull/wBusy thresholds.
    private async ValueTask OnGroupListAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (!RequireAuth(conn, MessageId.CS_GROUPLIST_REQ))
        {
            return;
        }

        await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        var userId = conn.State.UserId!.Value;

        var groups = await db.TGROUPs
            .Where(g => g.bStatus != TsvrStatusDisable) // exclude offline worlds
            .Select(g => new
            {
                g.bGroupID, g.bType, g.szNAME, g.bStatus, g.wFull, g.wBusy, g.dwMaxUser,
                CharCount = db.TALLCHARTABLEs.Count(c => c.bWorldID == g.bGroupID && c.dwUserID == userId && c.bDelete == 0),
                // Distinct-user count per group ≈ legacy m_mapCurrentUser. Computed live from
                // TALLCHARTABLE; the legacy in-memory counter is initialised from the same data
                // at startup and incremented/decremented on CREATECHAR/DELCHAR success.
                CurrentUsers = db.TALLCHARTABLEs
                    .Where(c => c.bWorldID == g.bGroupID && c.bDelete == 0)
                    .Select(c => c.dwUserID)
                    .Distinct()
                    .Count(),
            })
            .ToListAsync(ct).ConfigureAwait(false);

        // Bytes: 1 (CheckFilePoint) + 1 (bCount written via SetBuffer-copy in legacy; we
        // just write it once) + per-group (strName 4+N + 4 trailing bytes).
        var payloadSize = 2 + groups.Sum(g => 4 + Cp1252.Instance.GetByteCount(g.szNAME) + 4);
        var buf = new byte[payloadSize];
        var w = new PacketWriter(buf);
        w.WriteByte((byte)groups.Count);
        w.WriteByte(CheckFilePoint);
        foreach (var g in groups)
        {
            var wireStatus = ComputeWireStatus(g.bStatus, g.CurrentUsers, g.wFull, g.wBusy);

            // Live FULL override (CSHandler.cpp:526-534): if a max-user cap is set and the
            // pool is at or over it, and the user has NO existing char on this world, force
            // FULL — new users can't push the world over its cap.
            if (g.dwMaxUser > 0 && wireStatus != TstatusSleep && wireStatus != TstatusFull
                && g.CurrentUsers >= g.dwMaxUser && g.CharCount == 0)
            {
                wireStatus = TstatusFull;
            }

            w.WriteString(g.szNAME);
            w.WriteByte(g.bGroupID);
            w.WriteByte(g.bType);
            w.WriteByte(wireStatus);
            w.WriteByte((byte)Math.Min(g.CharCount, byte.MaxValue));
        }

        _logger.LogInformation("GROUPLIST_ACK to user={UserId}: {Count} groups", userId, groups.Count);
        await conn.Session.SendAsync(MessageId.CS_GROUPLIST_ACK, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);
    }

    // === Status enum mirror (Lib/Own/TProtocol/include/NetCode.h:931-946) ===
    private const byte TsvrStatusDisable = 0;
    // private const byte TsvrStatusEnable = 1;
    private const byte TsvrStatusSleep = 2;

    private const byte TstatusSleep = 0;
    private const byte TstatusNormal = 1;
    private const byte TstatusBusy = 2;
    private const byte TstatusFull = 3;

    private static byte ComputeWireStatus(byte dbStatus, int currentCount, short wFull, short wBusy)
    {
        if (dbStatus == TsvrStatusSleep)
        {
            return TstatusSleep;
        }
        if (currentCount > wFull)
        {
            return TstatusFull;
        }
        if (currentCount > wBusy)
        {
            return TstatusBusy;
        }
        return TstatusNormal;
    }

    // ===== CS_CHANNELLIST_REQ(bGroupID) → CS_CHANNELLIST_ACK (CSHandler.cpp:553-590) =====
    // Wire layout: BYTE bCount, BYTE CheckFilePoint, then repeated
    //   STRING szNAME, BYTE bChannel, BYTE bStatus
    private async ValueTask OnChannelListAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (!RequireAuth(conn, MessageId.CS_CHANNELLIST_REQ))
        {
            return;
        }

        var reader = new PacketReader(body.Span);
        var groupId = reader.ReadByte();
        conn.State.SelectedGroupId = groupId;

        await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        var channels = await db.TCHANNELs
            .Where(c => c.bGroupID == groupId && c.bStatus != TsvrStatusDisable)
            .OrderBy(c => c.bChannel)
            .Select(c => new { c.bChannel, c.szNAME, c.bStatus, c.wFull, c.wBusy })
            .ToListAsync(ct).ConfigureAwait(false);

        var payloadSize = 2 + channels.Sum(c => 4 + Cp1252.Instance.GetByteCount(c.szNAME) + 2);
        var buf = new byte[payloadSize];
        var w = new PacketWriter(buf);
        w.WriteByte((byte)channels.Count);
        w.WriteByte(CheckFilePoint);
        foreach (var c in channels)
        {
            // No live per-channel counter yet → currentCount=0 → NORMAL (unless DB says SLEEP).
            var wireStatus = ComputeWireStatus(c.bStatus, currentCount: 0, c.wFull, c.wBusy);
            w.WriteString(c.szNAME);
            w.WriteByte(c.bChannel);
            w.WriteByte(wireStatus);
        }

        _logger.LogInformation("CHANNELLIST_ACK group={Group}: {Count} channels", groupId, channels.Count);
        await conn.Session.SendAsync(MessageId.CS_CHANNELLIST_ACK, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);
    }

    // ===== CS_CHARLIST_REQ(bGroupID) → CS_CHARLIST_ACK =====
    // Wire layout (CSHandler.cpp:718-761 + CSSender flow):
    //   BYTE CheckFilePoint
    //   BYTE bCount
    //   repeated per char:
    //     DWORD dwCharID, STRING strName, BYTE bStartAct, bSlot, bLevel, bClass, bRace, bCountry,
    //     bSex, bHair, bFace, bBody, bPants, bHand, bFoot,
    //     DWORD dwRegion, dwFame, dwFameColor,
    //     BYTE bHelmetHide, bEquipItemCount,
    //     [items] BYTE bItemID, WORD wItemID, BYTE bLevel, BYTE bGradeEffect,
    //             WORD wColor, BYTE bRegGuild, WORD wMoggItemID
    private async ValueTask OnCharListAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (!RequireAuth(conn, MessageId.CS_CHARLIST_REQ))
        {
            return;
        }

        var reader = new PacketReader(body.Span);
        var groupId = reader.ReadByte();
        conn.State.SelectedGroupId = groupId;

        var userId = conn.State.UserId!.Value;
        await using var game = await _gameDbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);

        // Pull full per-char data from the per-world game DB (TCHARTABLE) plus guild fame
        // via TGUILDMEMBERTABLE / TGUILDTABLE join (mirrors CSPGetGuildInfo).
        var chars = await game.TCHARTABLEs
            .Where(c => c.dwUserID == userId && c.bDelete == 0)
            .OrderBy(c => c.bSlot)
            .Select(c => new
            {
                c.dwCharID, c.szNAME, c.bStartAct, c.bSlot, c.bLevel,
                c.bClass, c.bRace, c.bCountry, c.bSex,
                c.bHair, c.bFace, c.bBody, c.bPants, c.bHand, c.bFoot,
                c.dwRegion, c.bHelmetHide,
                Fame = (from m in game.TGUILDMEMBERTABLEs
                        join g in game.TGUILDTABLEs on m.dwGuildID equals g.dwID
                        where m.dwCharID == c.dwCharID
                        select new { g.dwFame, g.dwFameColor }).FirstOrDefault(),
            })
            .ToListAsync(ct).ConfigureAwait(false);

        // Equipped items per char in one round trip. Filter by STORAGE_INVEN+INVEN_EQUIP+TOWNER_CHAR
        // exactly as the legacy CTBLItem call does.
        var charIds = chars.Select(c => c.dwCharID).ToArray();
        var equipped = await game.TITEMTABLEs
            .Where(i => i.bStorageType == StorageInven
                && i.dwStorageID == InvenEquip
                && i.bOwnerType == TownerChar
                && charIds.Contains(i.dwOwnerID))
            .Select(i => new
            {
                i.dwOwnerID, i.bItemID, i.wItemID, i.bLevel, i.bGradeEffect, i.wMoggItemID,
            })
            .ToListAsync(ct).ConfigureAwait(false);
        var itemsByChar = equipped.GroupBy(i => i.dwOwnerID).ToDictionary(g => g.Key, g => g.ToList());

        var cp1252 = Cp1252.Instance;
        // Size: 1 (CheckFilePoint) + 1 (bCount) + per char
        //   4 + (4+name) + 14 single bytes + 12 (3 DWORDs) + 2 (helmetHide+bEquipCount) + items*(1+2+1+1+2+1+2)
        var perCharFixed = 4 + 14 + 12 + 2;
        int itemsPerChar(int charId) => itemsByChar.TryGetValue(charId, out var l) ? l.Count : 0;
        var payloadSize = 2 + chars.Sum(c => perCharFixed + (4 + cp1252.GetByteCount(c.szNAME))
            + itemsPerChar(c.dwCharID) * 10);

        var buf = new byte[payloadSize];
        var w = new PacketWriter(buf);
        w.WriteByte(CheckFilePoint);
        w.WriteByte((byte)chars.Count);
        foreach (var c in chars)
        {
            w.WriteUInt32((uint)c.dwCharID);
            w.WriteString(c.szNAME);
            w.WriteByte(c.bStartAct);
            w.WriteByte(c.bSlot);
            w.WriteByte(c.bLevel);
            w.WriteByte(c.bClass);
            w.WriteByte(c.bRace);
            w.WriteByte(c.bCountry);
            w.WriteByte(c.bSex);
            w.WriteByte(c.bHair);
            w.WriteByte(c.bFace);
            w.WriteByte(c.bBody);
            w.WriteByte(c.bPants);
            w.WriteByte(c.bHand);
            w.WriteByte(c.bFoot);
            w.WriteUInt32((uint)c.dwRegion);
            w.WriteUInt32((uint)(c.Fame?.dwFame ?? 0));
            w.WriteUInt32((uint)(c.Fame?.dwFameColor ?? 0));
            w.WriteByte(c.bHelmetHide);

            var items = itemsByChar.TryGetValue(c.dwCharID, out var list) ? list : [];
            w.WriteByte((byte)Math.Min(items.Count, byte.MaxValue));
            foreach (var it in items)
            {
                w.WriteByte(it.bItemID);
                w.WriteUInt16((ushort)it.wItemID);
                w.WriteByte(it.bLevel);
                w.WriteByte(it.bGradeEffect);
                // wColor / bRegGuild come from the legacy CTBLItem SP which we haven't ported;
                // both are cosmetic — wColor is dye, bRegGuild is the guild-stamp flag. Default 0.
                w.WriteUInt16(0);
                w.WriteByte(0);
                w.WriteUInt16((ushort)it.wMoggItemID);
            }
        }

        _logger.LogInformation(
            "CHARLIST_ACK user={UserId} group={Group}: {Count} chars, {Items} equipped items total",
            userId, groupId, chars.Count, equipped.Count);
        await conn.Session.SendAsync(MessageId.CS_CHARLIST_ACK, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);
    }

    // ===== CS_CREATECHAR_REQ → CS_CREATECHAR_ACK (PROTOCOL.md §4c) =====
    private async ValueTask OnCreateCharAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (!RequireAuth(conn, MessageId.CS_CREATECHAR_REQ))
        {
            return;
        }

        var r = new PacketReader(body.Span);
        var groupId = r.ReadByte();
        var name = r.ReadString();
        var slot = r.ReadByte();
        var cls = r.ReadByte();
        var race = r.ReadByte();
        var country = r.ReadByte();
        var sex = r.ReadByte();
        var hair = r.ReadByte();
        var face = r.ReadByte();
        var bodyB = r.ReadByte();
        var pants = r.ReadByte();
        var hand = r.ReadByte();
        var foot = r.ReadByte();
        // 13th trailing byte: veteran level option (CSHandler.cpp:994,1011). Picks which
        // entry of m_vVETERAN gets used to set the starting level. 0 = no veteran bonus.
        var bLevelOption = r.Remaining > 0 ? r.ReadByte() : (byte)0;

        // Legacy CSHandler.cpp:1013: bGroupID must match the pUser->m_bGroupID stamped during
        // CHARLIST. Defends against a client trying to spawn a char in a world it never selected.
        if (conn.State.SelectedGroupId != groupId)
        {
            _logger.LogWarning(
                "CREATECHAR groupId mismatch from {Ip}: packet={Pkt} session={Sess}",
                conn.RemoteAddress, groupId, conn.State.SelectedGroupId);
            return;
        }

        var userId = conn.State.UserId!.Value;

        // Resolve the starting level from bLevelOption per CSHandler.cpp:1083-1087:
        //   for each row in m_vVETERAN: if row.bID == bLevelOption → use row.bLevel.
        // bLevelOption=0 means "no veteran bonus" → start at level 1.
        byte startingLevel = 0;
        if (bLevelOption != 0)
        {
            startingLevel = await ResolveVeteranLevelAsync(bLevelOption, ct).ConfigureAwait(false);
        }
        var inPcBang = conn.State.InPcBang;

        CreateCharOutcome outcome;
        try
        {
            outcome = await _charService.CreateAsync(
                userId, groupId, slot, name, cls, race, country, sex,
                hair, face, bodyB, pants, hand, foot,
                startingLevel, inPcBang, ct).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "CharService.CreateAsync threw for user={UserId} name={Name}", userId, name);
            throw;
        }

        // Payload: bResult(1) + dwID(4) + strName(4+N) + 13 byte fields after the name
        //   (bSlotID, bClass, bRace, bCountry, bSex, bHair, bFace, bBody, bPants, bHand, bFoot,
        //    bCreateCnt, bLevel)
        // C++ reference: Server/TLoginSvr/CSSender.cpp:68-103 — sender takes
        // BYTE bCreateCnt, BYTE bLevel and writes both as the final trailing bytes.
        var nameBytes = Cp1252.Instance.GetByteCount(name);
        var buf = new byte[1 + 4 + 4 + nameBytes + 13];
        var w = new PacketWriter(buf);
        w.WriteByte((byte)outcome.Result);
        w.WriteUInt32((uint)outcome.CharId);
        w.WriteString(name);
        w.WriteByte(slot);
        w.WriteByte(cls);
        w.WriteByte(race);
        w.WriteByte(country);
        w.WriteByte(sex);
        w.WriteByte(hair);
        w.WriteByte(face);
        w.WriteByte(bodyB);
        w.WriteByte(pants);
        w.WriteByte(hand);
        w.WriteByte(foot);
        w.WriteByte(outcome.RemainingSlots);
        w.WriteByte(outcome.Level);

        _logger.LogInformation(
            "CREATECHAR_ACK user={UserId} world={World} name={Name} -> {Result} char={CharId}",
            userId, groupId, name, outcome.Result, outcome.CharId);
        await conn.Session.SendAsync(MessageId.CS_CREATECHAR_ACK, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);
    }

    // ===== CS_DELCHAR_REQ → CS_DELCHAR_ACK (PROTOCOL.md §4c) =====
    private async ValueTask OnDeleteCharAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        // Legacy DELCHAR checks pUser->m_dwID only (CSHandler.cpp:1210), not m_bAgreement.
        if (!RequireAuthIgnoreAgreement(conn, MessageId.CS_DELCHAR_REQ))
        {
            return;
        }

        var r = new PacketReader(body.Span);
        var groupId = r.ReadByte();
        var password = r.ReadString();
        var charId = (int)r.ReadUInt32();
        var userId = conn.State.UserId!.Value;

        // Legacy CSHandler.cpp:1223: enforce world consistency.
        if (conn.State.SelectedGroupId != groupId)
        {
            _logger.LogWarning(
                "DELCHAR groupId mismatch user={UserId}: packet={Pkt} session={Sess}",
                userId, groupId, conn.State.SelectedGroupId);
            return;
        }

        var outcome = await _charService.DeleteAsync(userId, groupId, charId, password, ct).ConfigureAwait(false);

        // Payload: bResult, dwCharID
        Span<byte> buf = stackalloc byte[1 + 4];
        var w = new PacketWriter(buf);
        w.WriteByte((byte)outcome.Result);
        w.WriteUInt32((uint)charId);

        _logger.LogInformation(
            "DELCHAR_ACK user={UserId} world={World} char={CharId} -> {Result}",
            userId, groupId, charId, outcome.Result);
        await conn.Session.SendAsync(MessageId.CS_DELCHAR_ACK, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);
    }

    // ===== CS_START_REQ(bGroupID, bChannel, dwCharID) → CS_START_ACK =====
    private async ValueTask OnStartAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (!RequireAuth(conn, MessageId.CS_START_REQ))
        {
            return;
        }

        var reader = new PacketReader(body.Span);
        var groupId = reader.ReadByte();
        var channel = reader.ReadByte();
        var charId = (int)reader.ReadUInt32();

        conn.State.SelectedGroupId = groupId;
        conn.State.SelectedChannel = channel;
        conn.State.SelectedCharId = charId;

        // BR override (CSHandler.cpp:1387-1398): if this char is registered in TBRPLAYERTABLE,
        // route to the BR (Battle Royale) shard. BOW table doesn't exist in this schema, so we
        // skip the BOW check — only BR is implemented here.
        byte? preferredServerId = null;
        await using (var game = await _gameDbFactory.CreateDbContextAsync(ct).ConfigureAwait(false))
        {
            var isBr = await game.TBRPLAYERTABLEs
                .AnyAsync(p => p.dwUserID == conn.State.UserId && p.dwCharID == charId, ct)
                .ConfigureAwait(false);
            if (isBr)
            {
                preferredServerId = BrServerId;
                channel = BrServerId; // legacy also overrides bChannel
                _logger.LogInformation(
                    "START: char={Char} is BR-registered → routing to ServerID={Sid}", charId, BrServerId);
            }
        }

        var endpoint = await _mapLocator.LookupAsync(groupId, preferredServerId, ct).ConfigureAwait(false);

        // Payload (PROTOCOL.md §4c): BYTE bResult, DWORD dwMapIP, WORD wPort, BYTE bServerID
        Span<byte> buf = stackalloc byte[1 + 4 + 2 + 1];
        var w = new PacketWriter(buf);
        w.WriteByte((byte)LoginResult.Success);
        w.WriteUInt32(IpToDword(endpoint.IpAddress));
        w.WriteUInt16(endpoint.Port);
        w.WriteByte(endpoint.ServerId);

        _logger.LogInformation(
            "START_ACK user={UserId} group={Group} ch={Channel} char={Char} -> {Ip}:{Port} (serverId={ServerId})",
            conn.State.UserId, groupId, channel, charId, endpoint.IpAddress, endpoint.Port, endpoint.ServerId);
        await conn.Session.SendAsync(MessageId.CS_START_ACK, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);

        // Legacy CSHandler.cpp:1428: pUser->m_bLogout = FALSE — disable disconnect-time cleanup.
        // The client is now transitioning to MapSvr with the same dwKEY; MapSvr will run TLogoutAll
        // when the gameplay session ends. If we wipe TCURRENTUSER on the TCP drop, MapSvr would
        // reject the new connection's dwKEY as unknown.
        conn.State.HandoffToMap = true;
    }

    // ===== CS_AGREEMENT_REQ → no ACK (TLogin.sql:238-239 / TAgreement SP) =====
    // Client sends `WORD wVersion` after EULA accept. Port of TAgreement.sql:
    //   IF EXISTS(TUSERINFOTABLE row) → UPDATE bAgreement = bAgreement + 1
    //   ELSE INSERT (dwUserID, bAgreement=1, dCabinetUse='2008-01-01')
    // The 7-day release-window cash-cabinet bonus from the legacy SP is intentionally
    // skipped — it's a launch-event freebie we don't carry forward.
    private async ValueTask OnAgreementAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (!RequireAuthIgnoreAgreement(conn, MessageId.CS_AGREEMENT_REQ))
        {
            return;
        }
        var r = new PacketReader(body.Span);
        var version = r.ReadUInt16();
        var userId = conn.State.UserId!.Value;

        await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        var existing = await db.TUSERINFOTABLEs
            .FirstOrDefaultAsync(u => u.dwUserID == userId, ct)
            .ConfigureAwait(false);

        if (existing is null)
        {
            db.TUSERINFOTABLEs.Add(new Persistence.Global.TUSERINFOTABLE
            {
                dwUserID = userId,
                bAgreement = 1,
                bCanCreateCharCount = 6,
                dCabinetUse = new DateTime(2008, 1, 1),
            });
        }
        else
        {
            // Increment rather than set=1 to preserve TAgreement.sql's semantics exactly
            // (the SP increments unconditionally on every accept).
            existing.bAgreement = (byte)Math.Min(existing.bAgreement + 1, byte.MaxValue);
        }
        await db.SaveChangesAsync(ct).ConfigureAwait(false);
        // Flip the in-memory gate so the next CS_LOGIN_REQ/GROUPLIST/etc. is accepted.
        conn.State.AgreementAccepted = true;

        _logger.LogInformation(
            "AGREEMENT user={UserId} version=0x{Version:X4} accepted (bAgreement now {Count})",
            userId, version, existing?.bAgreement ?? 1);
    }

    // ===== CS_HOTSEND_REQ — exec-file integrity heartbeat (no ACK) =====
    // Wire layout (CSHandler.cpp:1458-1486): INT64 dlValue, BYTE bAll.
    // Legacy validates dlValue against m_dlCheckFile ^ m_dlCheckKey only when the server
    // has an exec-file integrity hash configured (m_hExecFile != INVALID_HANDLE_VALUE). We
    // don't ship that feature, so reads consume the 9 bytes and we return without writing
    // any ACK — the legacy server NEVER replies to this packet.
    private ValueTask OnHotsendAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (!RequireAuthIgnoreAgreement(conn, MessageId.CS_HOTSEND_REQ))
        {
            return ValueTask.CompletedTask;
        }
        if (body.Length >= 9)
        {
            var r = new PacketReader(body.Span);
            var dlValue = r.ReadInt64();
            var bAll = r.ReadByte();
            _logger.LogDebug(
                "HOTSEND user={UserId} dlValue=0x{V:X16} bAll={All}",
                conn.State.UserId, dlValue, bAll);
        }
        // No ACK by design.
        return ValueTask.CompletedTask;
    }

    // ===== CS_VETERAN_REQ → CS_VETERAN_ACK — returning-player bonus options =====
    // Wire layout from CSSender.cpp:145-159 / CSHandler.cpp:1503:
    //   BYTE bOption, BYTE bFirstLevel, BYTE bSecondLevel, BYTE bThirdLevel
    // Legacy unconditionally sends bOption=3 ("all three options available") plus the
    // three threshold levels from the in-memory m_vVETERAN[0..2]. The eligibility check
    // (TCheckVeteran SP, comparing user's max char level) is commented out in the
    // legacy build — every user sees all three options and the client/server validate
    // the chosen bLevelOption at CREATECHAR time.
    private async ValueTask OnVeteranAsync(ClientConnection conn, ReadOnlyMemory<byte> _, CancellationToken ct)
    {
        if (!RequireAuthIgnoreAgreement(conn, MessageId.CS_VETERAN_REQ))
        {
            return;
        }

        await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        var rows = await db.TVETERANCHARTs
            .OrderBy(v => v.bID)
            .Select(v => v.bLevel)
            .Take(3)
            .ToListAsync(ct).ConfigureAwait(false);

        Span<byte> buf = stackalloc byte[4];
        buf[0] = 3; // bOption — always "all available"; matches legacy literal value
        buf[1] = rows.Count > 0 ? rows[0] : (byte)0;
        buf[2] = rows.Count > 1 ? rows[1] : (byte)0;
        buf[3] = rows.Count > 2 ? rows[2] : (byte)0;
        _logger.LogInformation(
            "VETERAN_ACK user={UserId} levels=[{L1},{L2},{L3}]",
            conn.State.UserId, buf[1], buf[2], buf[3]);
        await conn.Session.SendAsync(MessageId.CS_VETERAN_ACK, buf, ct).ConfigureAwait(false);
    }

    // ===== CS_SECURITYCONFIRM_ACK (client → server) =====
    // Direction is asymmetric — see CSSender.cpp:161 (server SENDS _REQ when LR_SECURITY
    // path triggers) and CSHandler.cpp:1508 (server RECEIVES _ACK with the code).
    // Wire layout: STRING strCode. Legacy compares case-insensitively against the per-session
    // m_strCode that was issued earlier, calls CSPAddNewMACAddress on match, then sends
    // CS_SECURITYRESULT_ACK with CODE_CORRECT or CODE_INCORRECT.
    //
    // The LR_SECURITY auth path is dead code in the shipped legacy build (CSHandler.cpp:371-389
    // is commented out), so no real client ever exercises this. We accept the packet, log it,
    // and reply with CODE_CORRECT (0) — placeholder until / unless we resurrect the flow.
    private const byte CodeCorrect = 0;
    private const byte CodeIncorrect = 1;

    private async ValueTask OnSecurityConfirmAckAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        if (!RequireAuthIgnoreAgreement(conn, MessageId.CS_SECURITYCONFIRM_ACK))
        {
            return;
        }
        var r = new PacketReader(body.Span);
        var code = r.ReadString();
        _logger.LogInformation(
            "SECURITYCONFIRM_ACK user={UserId} code={Code} — auto-accepting (LR_SECURITY path unused)",
            conn.State.UserId, code);

        Span<byte> buf = stackalloc byte[1];
        buf[0] = CodeCorrect;
        await conn.Session.SendAsync(MessageId.CS_SECURITYRESULT_ACK, buf, ct).ConfigureAwait(false);
    }

    /// <summary>
    /// Looks up the starting level for the chosen veteran option byte. Maps directly
    /// to legacy <c>m_vVETERAN</c> lookup (CSHandler.cpp:1083-1087): find the row whose
    /// <c>bID == bLevelOption</c> and return its <c>bLevel</c>. 0 if the option is not in
    /// the table.
    /// </summary>
    private async Task<byte> ResolveVeteranLevelAsync(byte bLevelOption, CancellationToken ct)
    {
        await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        return await db.TVETERANCHARTs
            .Where(v => v.bID == bLevelOption)
            .Select(v => (byte?)v.bLevel)
            .FirstOrDefaultAsync(ct)
            .ConfigureAwait(false) ?? 0;
    }

    /// <summary>
    /// Gate: must have a userId (post-LOGIN_ACK Success) AND the in-memory agreement flag.
    /// Mirrors legacy <c>if(!pUser->m_bAgreement) return EC_SESSION_INVALIDCHAR;</c> at the
    /// top of GROUPLIST/CHANNELLIST/CHARLIST/CREATECHAR/START.
    /// </summary>
    private bool RequireAuth(ClientConnection conn, MessageId id)
    {
        if (!conn.State.IsAuthenticated)
        {
            _logger.LogWarning("Rejecting {Id} from unauthenticated connection {Ip}", id, conn.RemoteAddress);
            return false;
        }
        if (!conn.State.AgreementAccepted)
        {
            _logger.LogWarning(
                "Rejecting {Id} from {Ip}: agreement not accepted (user={UserId})",
                id, conn.RemoteAddress, conn.State.UserId);
            return false;
        }
        return true;
    }

    /// <summary>
    /// Gate for handlers that legacy does NOT condition on m_bAgreement
    /// (HOTSEND, VETERAN, AGREEMENT, SECURITYCONFIRM_ACK, DELCHAR). Just needs auth.
    /// </summary>
    private bool RequireAuthIgnoreAgreement(ClientConnection conn, MessageId id)
    {
        if (conn.State.IsAuthenticated)
        {
            return true;
        }
        _logger.LogWarning("Rejecting {Id} from unauthenticated connection {Ip}", id, conn.RemoteAddress);
        return false;
    }

    private static uint IpToDword(string ipString)
    {
        if (!IPAddress.TryParse(ipString, out var ip))
        {
            return 0;
        }
        var bytes = ip.GetAddressBytes();
        if (bytes.Length != 4)
        {
            return 0;
        }
        return (uint)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
    }
}
