using FourStory.Map.Grains;
using FourStory.Map.Network;
using FourStory.Map.Services;
using FourStory.Protocol;
using Microsoft.Extensions.Logging;
using Orleans;

namespace FourStory.Map.Handlers;

/// <summary>
/// Handles <c>MW_ENTERSVR_REQ</c> (0x9008) — sent by World to Map after World
/// has loaded the persistent character row in response to our
/// <c>MW_ADDCHAR_ACK</c>. Carries the per-character snapshot needed to drive
/// <c>InitMap</c>.
///
/// C++ reference (the legacy reception side of this packet on Map):
/// <c>Server/TMapSvr/SSHandler.cpp:3072-3106</c>
/// (<c>CTMapSvrModule::OnMW_ENTERSVR_REQ</c>). The legacy handler stores the
/// arriving packet in <c>m_mainchar</c> for later replay through the DM round
/// trip; we don't have DM, so we parse the snapshot directly and stamp it onto
/// <see cref="MapSessionState"/>.
///
/// <para><b>Wire format.</b></para>
/// See <c>FourStory.World.Handlers.EnterSvrPacket</c> for the exact byte
/// layout. We are the *only* consumer of this packet so the producer/consumer
/// stay synchronised here.
///
/// <para><b>Pre/post-conditions.</b></para>
/// Routing requires that <see cref="MapConnectionRegistry"/> has the
/// charId → MapConnection mapping. <c>ConnectHandler</c> publishes that mapping
/// on successful CS_CONNECT_REQ, BEFORE Map sends <c>MW_ADDCHAR_ACK</c>. So by
/// the time this reply arrives, the registry entry exists. If it doesn't (the
/// client raced a disconnect, or the World->Map link delivered out of order
/// after a reconnect), we log and drop — analogous to the C++
/// <c>FindPlayer(dwCharID, dwKEY) == NULL</c> early return at SSHandler.cpp:3087.
/// </summary>
public sealed class EnterSvrHandler
{
    private readonly MapConnectionRegistry _registry;
    private readonly MapInitOrchestrator _init;
    private readonly IGrainFactory _grains;
    private readonly ILogger<EnterSvrHandler> _logger;

    public EnterSvrHandler(
        MapConnectionRegistry registry,
        MapInitOrchestrator init,
        IGrainFactory grains,
        ILogger<EnterSvrHandler> logger)
    {
        _registry = registry;
        _init = init;
        _grains = grains;
        _logger = logger;
    }

    public void Register(WorldClientDispatcher dispatcher)
    {
        dispatcher.Register(MessageId.MW_ENTERSVR_REQ, OnEnterSvrAsync);
    }

    private async ValueTask OnEnterSvrAsync(WorldClient _, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        var r = new PacketReader(body.Span);
        var dbLoad = r.ReadByte();
        var charId = (int)r.ReadUInt32();
        var key = r.ReadUInt32();

        if (!_registry.TryGet(charId, out var conn))
        {
            _logger.LogWarning(
                "MW_ENTERSVR_REQ char={CharId} key=0x{Key:X8}: no MapConnection in registry (already disconnected?) — dropping",
                charId, key);
            return;
        }

        // Defence-in-depth: the C++ FindPlayer matches on both dwID AND dwKEY,
        // so a stale snapshot delivered after a session was tore down and the
        // same charId is reused won't bleed into the new session.
        if (conn.State.Key is uint stored && stored != key)
        {
            _logger.LogWarning(
                "MW_ENTERSVR_REQ char={CharId} key mismatch (got 0x{Got:X8}, session has 0x{Stored:X8}) — dropping",
                charId, key, stored);
            return;
        }

        if (dbLoad != 0)
        {
            // The C# port should never see bDBLoad=1 — World sends only the
            // inline-data form (see FourStory.World.Handlers.EnterSvrPacket).
            // The legacy bDBLoad=1 form means "ask DM to load", which would
            // leave us hanging because we have no DM. Log loudly so a future
            // re-introduction of the DM peer doesn't silently regress.
            _logger.LogError(
                "MW_ENTERSVR_REQ char={CharId} bDBLoad=1 — unexpected in C# port (no DM peer); ignoring",
                charId);
            return;
        }

        // ------------------------ snapshot parse ------------------------
        // Layout matches FourStory.World.Handlers.EnterSvrPacket. Order is
        // load-bearing — do not reorder without updating the producer.
        var name = r.ReadString();
        var startAct = r.ReadByte();
        var realSex = r.ReadByte();
        var cls = r.ReadByte();
        var level = r.ReadByte();
        var race = r.ReadByte();
        var country = r.ReadByte();
        var oriCountry = r.ReadByte();
        var sex = r.ReadByte();
        var hair = r.ReadByte();
        var face = r.ReadByte();
        var body2 = r.ReadByte();
        var pants = r.ReadByte();
        var hand = r.ReadByte();
        var foot = r.ReadByte();
        var helmetHide = r.ReadByte();
        var gold = r.ReadInt32();
        var silver = r.ReadInt32();
        var cooper = r.ReadInt32();
        var exp = r.ReadInt32();
        var hp = r.ReadInt32();
        var mp = r.ReadInt32();
        var skillPoint = r.ReadInt16();
        var region = r.ReadInt32();
        var guildLeave = r.ReadByte();
        var guildLeaveTime = r.ReadInt32();
        var mapId = r.ReadInt16();
        var spawnId = r.ReadInt16();
        var lastSpawnId = r.ReadInt16();
        var lastDest = r.ReadInt32();
        var temptedMon = r.ReadInt16();
        var aftermath = r.ReadByte();
        var posX = r.ReadSingle();
        var posY = r.ReadSingle();
        var posZ = r.ReadSingle();
        var dir = r.ReadInt16();
        var statLevel = r.ReadByte();
        var statPoint = r.ReadByte();
        var statExp = r.ReadInt32();
        var rankPoint = r.ReadInt32();

        var s = conn.State;
        s.Name = name;
        s.StartAct = startAct;
        s.RealSex = realSex;
        s.Class = cls;
        s.Level = level;
        s.Race = race;
        s.Country = country;
        s.OriCountry = oriCountry;
        s.Sex = sex;
        s.Hair = hair;
        s.Face = face;
        s.Body = body2;
        s.Pants = pants;
        s.Hand = hand;
        s.Foot = foot;
        s.HelmetHide = helmetHide;
        s.Gold = gold;
        s.Silver = silver;
        s.Cooper = cooper;
        s.Exp = exp;
        s.HP = hp;
        s.MP = mp;
        s.SkillPoint = skillPoint;
        s.Region = region;
        s.GuildLeave = guildLeave;
        s.GuildLeaveTime = guildLeaveTime;
        s.MapId = mapId;
        s.SpawnId = spawnId;
        s.LastSpawnId = lastSpawnId;
        s.LastDestination = lastDest;
        s.TemptedMon = temptedMon;
        s.Aftermath = aftermath;
        s.PosX = posX;
        s.PosY = posY;
        s.PosZ = posZ;
        s.Dir = dir;
        s.StatLevel = statLevel;
        s.StatPoint = statPoint;
        s.StatExp = statExp;
        s.RankPoint = rankPoint;
        s.HasSnapshot = true;

        // Persist the snapshot to the player's grain. The grain is durable across
        // session/reconnect; the MapSessionState above is the per-connection mirror.
        var playerGrain = _grains.GetGrain<IPlayerGrain>(charId);
        var snapshot = new PlayerSnapshot
        {
            CharId = charId,
            UserId = conn.State.UserId ?? 0,
            Name = name,
            StartAct = startAct,
            Class = cls,
            Race = race,
            Country = country,
            OriCountry = oriCountry,
            Sex = sex,
            RealSex = realSex,
            Hair = hair,
            Face = face,
            Body = body2,
            Pants = pants,
            Hand = hand,
            Foot = foot,
            HelmetHide = helmetHide,
            Level = level,
            Gold = gold,
            Silver = silver,
            Cooper = cooper,
            Exp = exp,
            HP = hp,
            MP = mp,
            SkillPoint = skillPoint,
            Region = region,
            GuildLeave = guildLeave,
            GuildLeaveTime = guildLeaveTime,
            MapId = mapId,
            SpawnId = spawnId,
            LastSpawnId = lastSpawnId,
            LastDestination = lastDest,
            TemptedMon = temptedMon,
            Aftermath = aftermath,
            PosX = posX,
            PosY = posY,
            PosZ = posZ,
            Dir = dir,
            StatLevel = statLevel,
            StatPoint = statPoint,
            StatExp = statExp,
            RankPoint = rankPoint,
            Channel = conn.State.Channel ?? 0,
            WorldId = 1, // TODO: thread MapServerInfo.GroupId through to here
        };
        await playerGrain.SetSnapshotAsync(snapshot);
        await playerGrain.EnterMapAsync(mapId, conn.State.Channel ?? 0, 1);

        _logger.LogInformation(
            "MW_ENTERSVR_REQ char={CharId} '{Name}' map={MapId} lvl={Level} country={Country} pos=({X:F2},{Y:F2},{Z:F2}) — snapshot stored to grain",
            charId, name, mapId, level, country, posX, posY, posZ);

        // ------------------------ InitMap trigger ------------------------
        // In C++ the InitMap fan-out runs when CS_CONREADY_REQ comes in
        // (CSHandler.cpp:402 calls InitMap when m_pMAP is null). It can
        // therefore race the DM-loaded char data either way — and the legacy
        // code's m_bExit / m_pMAP nullness checks paper over that.
        //
        // We make it explicit: InitMap requires both
        //   (a) IsReady — CS_CONREADY_REQ has arrived from the client, AND
        //   (b) HasSnapshot — World has delivered the char snapshot.
        // Whichever of these two flips last triggers the fan-out. Below is
        // the "snapshot arrives second" branch; the symmetric branch lives
        // in ReadyHandler (it sees HasSnapshot already true).
        // Fan-out fires when both flags are set. The orchestrator is idempotent —
        // safe whether this branch runs first (snapshot before ready) or second
        // (snapshot after ready, ReadyHandler already called it).
        await _init.TryFireAsync(conn, ct).ConfigureAwait(false);
    }
}
