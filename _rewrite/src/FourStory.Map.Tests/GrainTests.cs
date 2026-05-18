using FourStory.Map.Grains;
using Orleans.Hosting;
using Orleans.TestingHost;

namespace FourStory.Map.Tests;

/// <summary>
/// In-process Orleans test cluster (<see cref="TestCluster"/>) — spins up a real
/// silo with in-memory storage and exercises grain code end-to-end. Slow to
/// start (~3s) so we wrap it in a fixture and share across tests.
/// </summary>
public sealed class OrleansFixture : IAsyncLifetime
{
    public TestCluster Cluster { get; private set; } = null!;

    public async Task InitializeAsync()
    {
        var builder = new TestClusterBuilder();
        builder.AddSiloBuilderConfigurator<TestSiloConfig>();
        Cluster = builder.Build();
        await Cluster.DeployAsync();
    }

    public Task DisposeAsync() => Cluster.StopAllSilosAsync();

    private sealed class TestSiloConfig : ISiloConfigurator
    {
        public void Configure(ISiloBuilder siloBuilder)
        {
            siloBuilder.AddMemoryGrainStorageAsDefault();
        }
    }
}

public sealed class GrainTests : IClassFixture<OrleansFixture>
{
    private readonly OrleansFixture _f;

    public GrainTests(OrleansFixture f) => _f = f;

    [Fact]
    public async Task PlayerGrain_SetSnapshot_RoundTrips()
    {
        var grain = _f.Cluster.GrainFactory.GetGrain<IPlayerGrain>(1001);
        var snap = new PlayerSnapshot
        {
            CharId = 1001,
            Name = "Alice",
            Level = 42,
            HP = 1234,
            MP = 567,
            PosX = 100.5f,
            PosY = 0f,
            PosZ = 200.25f,
        };
        await grain.SetSnapshotAsync(snap);

        var read = await grain.GetSnapshotAsync();
        Assert.Equal("Alice", read.Name);
        Assert.Equal(42, read.Level);
        Assert.Equal(1234, read.HP);
        Assert.Equal(100.5f, read.PosX);
    }

    [Fact]
    public async Task PlayerGrain_EnterMap_JoinsMapGrain()
    {
        var charId = 2002;
        var player = _f.Cluster.GrainFactory.GetGrain<IPlayerGrain>(charId);
        await player.SetSnapshotAsync(new PlayerSnapshot { CharId = charId, Name = "Bob" });

        await player.EnterMapAsync(mapId: 2010, channel: 1, worldId: 1);

        var map = _f.Cluster.GrainFactory.GetGrain<IMapGrain>(MapGrainKey.For(1, 1, 2010));
        var count = await map.GetPlayerCountAsync();
        Assert.Equal(1, count);

        var neighbors = await map.GetNeighborsAsync(charId);
        Assert.Empty(neighbors); // alone
    }

    [Fact]
    public async Task MapGrain_TwoPlayers_SeeEachOther()
    {
        var a = _f.Cluster.GrainFactory.GetGrain<IPlayerGrain>(3001);
        var b = _f.Cluster.GrainFactory.GetGrain<IPlayerGrain>(3002);
        await a.SetSnapshotAsync(new PlayerSnapshot { CharId = 3001, Name = "A" });
        await b.SetSnapshotAsync(new PlayerSnapshot { CharId = 3002, Name = "B" });

        await a.EnterMapAsync(2020, 1, 1);
        await b.EnterMapAsync(2020, 1, 1);

        var map = _f.Cluster.GrainFactory.GetGrain<IMapGrain>(MapGrainKey.For(1, 1, 2020));
        Assert.Equal(2, await map.GetPlayerCountAsync());
        Assert.Equal(new[] { 3002 }, await map.GetNeighborsAsync(3001));
        Assert.Equal(new[] { 3001 }, await map.GetNeighborsAsync(3002));
    }

    [Fact]
    public async Task PlayerGrain_LeaveMap_RemovesFromMapGrain()
    {
        var charId = 4001;
        var p = _f.Cluster.GrainFactory.GetGrain<IPlayerGrain>(charId);
        await p.SetSnapshotAsync(new PlayerSnapshot { CharId = charId });
        await p.EnterMapAsync(2030, 1, 1);

        var map = _f.Cluster.GrainFactory.GetGrain<IMapGrain>(MapGrainKey.For(1, 1, 2030));
        Assert.Equal(1, await map.GetPlayerCountAsync());

        await p.LeaveMapAsync();
        Assert.Equal(0, await map.GetPlayerCountAsync());
    }

    [Fact]
    public async Task PlayerGrain_EnterDifferentMap_LeavesPreviousAutomatically()
    {
        var charId = 5001;
        var p = _f.Cluster.GrainFactory.GetGrain<IPlayerGrain>(charId);
        await p.SetSnapshotAsync(new PlayerSnapshot { CharId = charId });

        await p.EnterMapAsync(2040, 1, 1);
        var mapA = _f.Cluster.GrainFactory.GetGrain<IMapGrain>(MapGrainKey.For(1, 1, 2040));
        Assert.Equal(1, await mapA.GetPlayerCountAsync());

        await p.EnterMapAsync(2050, 1, 1);
        var mapB = _f.Cluster.GrainFactory.GetGrain<IMapGrain>(MapGrainKey.For(1, 1, 2050));
        Assert.Equal(0, await mapA.GetPlayerCountAsync()); // auto-left A
        Assert.Equal(1, await mapB.GetPlayerCountAsync());
    }

    [Fact]
    public async Task MonsterGrain_SpawnAndDamage()
    {
        var m = _f.Cluster.GrainFactory.GetGrain<IMonsterGrain>(9001);
        await m.SpawnAsync(new MonsterSpawnInfo
        {
            MonsterId = 100,
            MapId = 2010,
            Channel = 1,
            PosX = 50f,
            PosY = 0f,
            PosZ = 50f,
            MaxHp = 500,
        });

        await m.TakeDamageAsync(150, attackerCharId: 1);
        var snap = await m.GetSnapshotAsync();
        Assert.Equal(350, snap.Hp);
        Assert.False(snap.IsDead);

        await m.TakeDamageAsync(400, attackerCharId: 1);
        snap = await m.GetSnapshotAsync();
        Assert.Equal(0, snap.Hp);
        Assert.True(snap.IsDead);
    }
}
