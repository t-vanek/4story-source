using Microsoft.Extensions.Logging;
using Orleans;

namespace FourStory.Map.Grains;

/// <summary>
/// Implementation of <see cref="IMonsterGrain"/>. Minimal skeleton — AI is a
/// no-op (idle state) until movement / combat handlers exist to react to.
/// </summary>
public sealed class MonsterGrain : Grain, IMonsterGrain
{
    private readonly ILogger<MonsterGrain> _logger;
    private MonsterSnapshot _snapshot = new();

    public MonsterGrain(ILogger<MonsterGrain> logger)
    {
        _logger = logger;
    }

    public Task SpawnAsync(MonsterSpawnInfo info)
    {
        _snapshot = new MonsterSnapshot
        {
            InstanceId = this.GetPrimaryKeyLong(),
            MonsterId = info.MonsterId,
            Hp = info.MaxHp,
            MaxHp = info.MaxHp,
            PosX = info.PosX,
            PosY = info.PosY,
            PosZ = info.PosZ,
            IsDead = false,
        };
        _logger.LogInformation(
            "Monster {InstanceId} ({MonsterId}) spawned at ({X:F2},{Y:F2},{Z:F2}) HP={Hp}",
            _snapshot.InstanceId, _snapshot.MonsterId, _snapshot.PosX, _snapshot.PosY, _snapshot.PosZ, _snapshot.Hp);
        return Task.CompletedTask;
    }

    public Task<MonsterSnapshot> GetSnapshotAsync() => Task.FromResult(_snapshot);

    public Task TakeDamageAsync(int amount, int attackerCharId)
    {
        if (_snapshot.IsDead || amount <= 0)
        {
            return Task.CompletedTask;
        }
        var newHp = Math.Max(0, _snapshot.Hp - amount);
        _snapshot = _snapshot with { Hp = newHp, IsDead = newHp == 0 };
        _logger.LogInformation(
            "Monster {InstanceId} took {Dmg} from char {Attacker}, HP={Hp}/{Max}",
            _snapshot.InstanceId, amount, attackerCharId, _snapshot.Hp, _snapshot.MaxHp);
        return Task.CompletedTask;
    }
}
