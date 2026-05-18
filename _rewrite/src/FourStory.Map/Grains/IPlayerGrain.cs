namespace FourStory.Map.Grains;

/// <summary>
/// Authoritative actor for a single active character. Replaces the legacy
/// <c>CTPlayer</c> (Server/TMapSvr/TPlayer.cpp, 7833 LOC) — but instead of being
/// owned by a TCP session, it lives in the Orleans silo and outlives a single
/// reconnect.
///
/// Key: charId (int) — natural primary key from <c>TCHARTABLE</c>.
///
/// Lifecycle:
/// <list type="bullet">
///   <item><see cref="EnterMapAsync"/> when CS_CONREADY fires + snapshot present.</item>
///   <item><see cref="LeaveMapAsync"/> when the TCP session closes.</item>
///   <item>The grain itself is deactivated by Orleans when idle past the timeout
///     (default 2h) — its state persists via the configured state provider.</item>
/// </list>
///
/// <para><b>Scope of this initial port.</b></para>
/// Carries the snapshot fields previously on <c>MapSessionState</c> (name, level,
/// position, currency, ...). Methods are stubs that return success but don't yet
/// drive AOI updates — that comes when the gameplay handlers land.
/// </summary>
public interface IPlayerGrain : Orleans.IGrainWithIntegerKey
{
    /// <summary>Stamp the freshly-arrived char snapshot into the grain.</summary>
    Task SetSnapshotAsync(PlayerSnapshot snapshot);

    /// <summary>Join the player to a map/channel. Idempotent.</summary>
    Task EnterMapAsync(short mapId, byte channel, byte worldId);

    /// <summary>Detach from the current map. Called on disconnect.</summary>
    Task LeaveMapAsync();

    /// <summary>Returns the current snapshot — used by Map handlers to build CS_CHARINFO_ACK etc.</summary>
    Task<PlayerSnapshot> GetSnapshotAsync();

    /// <summary>Move to a new position. AOI broadcast deferred to MapGrain.</summary>
    Task MoveAsync(float x, float y, float z, short dir);
}

/// <summary>
/// All scalar fields the grain holds. Snapshot is sent over the wire / used to
/// populate CS_CHARINFO_ACK; <see cref="MapSessionState"/> will eventually
/// delegate to the grain rather than store its own copy.
/// </summary>
[Orleans.GenerateSerializer]
public sealed record PlayerSnapshot
{
    [Orleans.Id(0)]  public int CharId { get; init; }
    [Orleans.Id(1)]  public int UserId { get; init; }
    [Orleans.Id(2)]  public string Name { get; init; } = string.Empty;
    [Orleans.Id(3)]  public byte StartAct { get; init; }
    [Orleans.Id(4)]  public byte Class { get; init; }
    [Orleans.Id(5)]  public byte Race { get; init; }
    [Orleans.Id(6)]  public byte Country { get; init; }
    [Orleans.Id(7)]  public byte OriCountry { get; init; }
    [Orleans.Id(8)]  public byte Sex { get; init; }
    [Orleans.Id(9)]  public byte RealSex { get; init; }
    [Orleans.Id(10)] public byte Hair { get; init; }
    [Orleans.Id(11)] public byte Face { get; init; }
    [Orleans.Id(12)] public byte Body { get; init; }
    [Orleans.Id(13)] public byte Pants { get; init; }
    [Orleans.Id(14)] public byte Hand { get; init; }
    [Orleans.Id(15)] public byte Foot { get; init; }
    [Orleans.Id(16)] public byte HelmetHide { get; init; }
    [Orleans.Id(17)] public byte Level { get; init; }
    [Orleans.Id(18)] public int Gold { get; init; }
    [Orleans.Id(19)] public int Silver { get; init; }
    [Orleans.Id(20)] public int Cooper { get; init; }
    [Orleans.Id(21)] public int Exp { get; init; }
    [Orleans.Id(22)] public int HP { get; init; }
    [Orleans.Id(23)] public int MP { get; init; }
    [Orleans.Id(24)] public short SkillPoint { get; init; }
    [Orleans.Id(25)] public int Region { get; init; }
    [Orleans.Id(26)] public byte GuildLeave { get; init; }
    [Orleans.Id(27)] public int GuildLeaveTime { get; init; }
    [Orleans.Id(28)] public short MapId { get; init; }
    [Orleans.Id(29)] public short SpawnId { get; init; }
    [Orleans.Id(30)] public short LastSpawnId { get; init; }
    [Orleans.Id(31)] public int LastDestination { get; init; }
    [Orleans.Id(32)] public short TemptedMon { get; init; }
    [Orleans.Id(33)] public byte Aftermath { get; init; }
    [Orleans.Id(34)] public float PosX { get; init; }
    [Orleans.Id(35)] public float PosY { get; init; }
    [Orleans.Id(36)] public float PosZ { get; init; }
    [Orleans.Id(37)] public short Dir { get; init; }
    [Orleans.Id(38)] public byte StatLevel { get; init; }
    [Orleans.Id(39)] public byte StatPoint { get; init; }
    [Orleans.Id(40)] public int StatExp { get; init; }
    [Orleans.Id(41)] public int RankPoint { get; init; }
    [Orleans.Id(42)] public byte Channel { get; init; }
    [Orleans.Id(43)] public byte WorldId { get; init; }
}
