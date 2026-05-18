using System.Net;
using FourStory.Map.Network;
using FourStory.Persistence;
using FourStory.Protocol;
using FourStory.Shared;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;

namespace FourStory.Map.Services;

/// <summary>
/// Drives the post-handshake fan-out that puts the client visually in the world.
/// Equivalent to <c>CTMapSvrModule::InitMap</c> (Server/TMapSvr/TMapSvr.cpp:7909) +
/// the first wave of SC packets <c>EnterMAP</c> dispatches.
///
/// Trigger condition (idempotent — only runs once per session):
///   <see cref="MapSessionState.IsReady"/> &amp;&amp; <see cref="MapSessionState.HasSnapshot"/>
///
/// First wave we send today:
/// <list type="number">
///   <item><c>CS_ADDCONNECT_ACK</c> — list of other Map endpoints in this world group.</item>
///   <item><c>CS_CHARINFO_ACK</c> — full self snapshot (≈90 fields).</item>
/// </list>
///
/// Deferred (no data yet):
/// <list type="bullet">
///   <item><c>CS_ENTER_ACK</c> per neighbouring player — needs AOI / spatial subsystem.</item>
///   <item>Item / skill / quest / companion bundles — Phase 3 (gameplay state) work.</item>
/// </list>
///
/// Each empty-count slot is still serialized (BYTE = 0) so the client's parser
/// accepts the packet and stays in sync.
/// </summary>
public sealed class MapInitOrchestrator
{
    private readonly IDbContextFactory<GlobalDbContext> _globalFactory;
    private readonly ILogger<MapInitOrchestrator> _logger;

    public MapInitOrchestrator(
        IDbContextFactory<GlobalDbContext> globalFactory,
        ILogger<MapInitOrchestrator> logger)
    {
        _globalFactory = globalFactory;
        _logger = logger;
    }

    /// <summary>
    /// Runs the fan-out if both prerequisites are met. Safe to call from either
    /// <c>ReadyHandler</c> or <c>EnterSvrHandler</c> — whichever flips the
    /// second flag wins, and <see cref="MapSessionState.InitMapDone"/> guards
    /// against double-fires.
    /// </summary>
    public async Task TryFireAsync(MapConnection conn, CancellationToken ct)
    {
        var s = conn.State;
        if (!s.IsReady || !s.HasSnapshot)
        {
            return;
        }
        if (s.InitMapDone)
        {
            return;
        }
        s.InitMapDone = true; // claim early; failures below just leak a flag, not double-fire

        _logger.LogInformation(
            "InitMap fan-out: char={CharId} '{Name}' map={MapId} pos=({X:F2},{Y:F2},{Z:F2})",
            s.CharId, s.Name, s.MapId, s.PosX, s.PosY, s.PosZ);

        await SendAddConnectAckAsync(conn, ct).ConfigureAwait(false);
        await SendCharInfoAckAsync(conn, ct).ConfigureAwait(false);
    }

    // ===== CS_ADDCONNECT_ACK (PROTOCOL.md §4c) =====
    //   BYTE bCount, { DWORD dwIPAddr, WORD wPort, BYTE bServerID }*count
    // Lists other Map servers the client can connect to (e.g., for channel hops).
    private async Task SendAddConnectAckAsync(MapConnection conn, CancellationToken ct)
    {
        await using var db = await _globalFactory.CreateDbContextAsync(ct).ConfigureAwait(false);

        var ourServerId = conn.State.MapId is short ? (byte)0 : (byte)0; // placeholder
        var rows = await db.TSERVERs
            .Where(s => s.bType == (byte)ServerType.MapSvr)
            .Join(db.TIPADDRs,
                s => s.bMachineID,
                ip => ip.bMachineID,
                (s, ip) => new { s.bServerID, s.wPort, ip.szIPAddr, ip.bActive })
            .Where(x => x.bActive != 0 && x.bServerID != ourServerId)
            .ToListAsync(ct).ConfigureAwait(false);

        // Payload: 1 + count * (4 + 2 + 1) = 1 + count * 7
        var buf = new byte[1 + rows.Count * 7];
        var w = new PacketWriter(buf);
        w.WriteByte((byte)rows.Count);
        foreach (var r in rows)
        {
            w.WriteUInt32(IpToDword(r.szIPAddr ?? "127.0.0.1"));
            w.WriteUInt16((ushort)r.wPort);
            w.WriteByte(r.bServerID);
        }
        await conn.Session.SendAsync(MessageId.CS_ADDCONNECT_ACK, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);
    }

    // ===== CS_CHARINFO_ACK (PROTOCOL.md §4c) =====
    // Big self-snapshot. We send the scalar header fields populated from
    // MapSessionState, plus zero-count for every list field (inventory, skills,
    // maintain-skills, party members, hotkeys, used items). The client parser
    // requires every count byte to be present, but a 0 means "skip the loop"
    // so we don't need to allocate per-list payload.
    private static async Task SendCharInfoAckAsync(MapConnection conn, CancellationToken ct)
    {
        var s = conn.State;
        var name = s.Name ?? string.Empty;

        // Conservative upper bound: 256 bytes for scalars + name bytes.
        var nameBytes = System.Text.Encoding.GetEncoding(1252).GetByteCount(name);
        var buf = new byte[256 + nameBytes];
        var w = new PacketWriter(buf);

        w.WriteUInt32((uint)(s.CharId ?? 0));
        w.WriteString(name);
        w.WriteByte(s.StartAct ?? 0);
        w.WriteByte(s.Class ?? 0);
        w.WriteByte(s.Race ?? 0);
        w.WriteByte(s.Country ?? 0);
        w.WriteByte(s.OriCountry ?? 0);            // bAidCountry — legacy reuses OriCountry slot
        w.WriteByte(s.Sex ?? 0);
        w.WriteByte(s.Hair ?? 0);
        w.WriteByte(s.Face ?? 0);
        w.WriteByte(s.Body ?? 0);
        w.WriteByte(s.Pants ?? 0);
        w.WriteByte(s.Hand ?? 0);
        w.WriteByte(s.Foot ?? 0);
        w.WriteByte(s.HelmetHide ?? 0);
        w.WriteByte(s.Level ?? 1);

        w.WriteUInt16(0);     // wPartyID — no party yet
        w.WriteUInt32(0);     // dwGuildID
        w.WriteUInt32(0);     // dwFame
        w.WriteUInt32(0);     // dwFameColor
        w.WriteByte(0);       // bGuildDuty
        w.WriteByte(0);       // bGuildPeer
        w.WriteString(string.Empty); // strGuildName
        w.WriteUInt32(0);     // dwTacticsID
        w.WriteString(string.Empty); // strTacticsName

        w.WriteUInt32((uint)(s.Gold ?? 0));
        w.WriteUInt32((uint)(s.Silver ?? 0));
        w.WriteUInt32((uint)(s.Cooper ?? 0));
        w.WriteUInt32(0);                          // dwPrevExp — TODO: from TLevelChart
        w.WriteUInt32(0);                          // dwNextExp — TODO: from TLevelChart
        w.WriteUInt32((uint)(s.Exp ?? 0));
        w.WriteUInt32((uint)(s.HP ?? 0));          // dwMaxHP — currently same as HP (no derived stats yet)
        w.WriteUInt32((uint)(s.HP ?? 0));          // dwHP
        w.WriteUInt32((uint)(s.MP ?? 0));          // dwMaxMP
        w.WriteUInt32((uint)(s.MP ?? 0));          // dwMP

        w.WriteUInt32(0);     // dwPartyChiefID
        w.WriteUInt16(0);     // wCommanderID
        w.WriteUInt32((uint)(s.Region ?? 0));
        w.WriteUInt16((ushort)(s.MapId ?? 0));
        w.WriteSingle(s.PosX ?? 0f);
        w.WriteSingle(s.PosY ?? 0f);
        w.WriteSingle(s.PosZ ?? 0f);
        w.WriteUInt16((ushort)(s.Dir ?? 0));
        w.WriteUInt16((ushort)(s.SkillPoint ?? 0)); // wMySkillPoint
        w.WriteByte(0);       // bLuckyNumber
        w.WriteUInt16(0);     // wSkillPoint_1..4 (4× short)
        w.WriteUInt16(0);
        w.WriteUInt16(0);
        w.WriteUInt16(0);

        // Empty lists — every count is BYTE so the parser skips its body.
        w.WriteByte(0);       // bInvenCount
        w.WriteByte(0);       // bSkillCount
        w.WriteByte(0);       // bMaintainSkillCount
        w.WriteByte(0);       // bPartyMemberCount
        w.WriteByte(0);       // bHotkeyCount
        w.WriteByte(0);       // bUsedItemCount

        w.WriteUInt32(0);     // dwPvPTotalPoint
        w.WriteUInt32(0);     // dwPvPUseablePoint

        await conn.Session.SendAsync(MessageId.CS_CHARINFO_ACK, w.WrittenSpan[..w.Position], ct).ConfigureAwait(false);
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
