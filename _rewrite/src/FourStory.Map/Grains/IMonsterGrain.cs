namespace FourStory.Map.Grains;

/// <summary>
/// Authoritative actor for a single spawned monster. Replaces the legacy
/// <c>CTMonster</c> + <c>CTMonsterAI</c> + 13× <c>TAICmd*.cpp</c>
/// (state-machine commands: Roam / Attack / Follow / Getaway / Gohome / ...).
///
/// Key: monster instance id (long) — globally unique across the cluster,
/// allocated by the map grain at spawn time.
///
/// <para><b>Current scope.</b></para>
/// Skeleton only. Holds template id + current HP + position. AI is a no-op
/// (state = Idle). Will be expanded once player movement + combat handlers
/// land, since AI reactions need observable player events to drive them.
/// </summary>
public interface IMonsterGrain : Orleans.IGrainWithIntegerKey
{
    Task SpawnAsync(MonsterSpawnInfo info);
    Task<MonsterSnapshot> GetSnapshotAsync();
    Task TakeDamageAsync(int amount, int attackerCharId);
}

[Orleans.GenerateSerializer]
public sealed record MonsterSpawnInfo
{
    [Orleans.Id(0)] public short MonsterId { get; init; }    // wMonID — TMONSTERCHART.wID
    [Orleans.Id(1)] public short MapId { get; init; }
    [Orleans.Id(2)] public byte Channel { get; init; }
    [Orleans.Id(3)] public float PosX { get; init; }
    [Orleans.Id(4)] public float PosY { get; init; }
    [Orleans.Id(5)] public float PosZ { get; init; }
    [Orleans.Id(6)] public int MaxHp { get; init; }
}

[Orleans.GenerateSerializer]
public sealed record MonsterSnapshot
{
    [Orleans.Id(0)] public long InstanceId { get; init; }
    [Orleans.Id(1)] public short MonsterId { get; init; }
    [Orleans.Id(2)] public int Hp { get; init; }
    [Orleans.Id(3)] public int MaxHp { get; init; }
    [Orleans.Id(4)] public float PosX { get; init; }
    [Orleans.Id(5)] public float PosY { get; init; }
    [Orleans.Id(6)] public float PosZ { get; init; }
    [Orleans.Id(7)] public bool IsDead { get; init; }
}
