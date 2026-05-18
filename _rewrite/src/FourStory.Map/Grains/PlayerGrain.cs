using Microsoft.Extensions.Logging;
using Orleans;

namespace FourStory.Map.Grains;

/// <summary>
/// Implementation of <see cref="IPlayerGrain"/>. State stored via
/// <see cref="IPersistentState{T}"/> — for dev the configured provider is
/// in-memory, in production this becomes Postgres / Redis.
/// </summary>
public sealed class PlayerGrain : Grain, IPlayerGrain
{
    private readonly IPersistentState<PlayerSnapshot> _state;
    private readonly ILogger<PlayerGrain> _logger;

    private IMapGrain? _currentMap;
    private string? _currentMapKey;

    public PlayerGrain(
        [PersistentState("snapshot", "Default")] IPersistentState<PlayerSnapshot> state,
        ILogger<PlayerGrain> logger)
    {
        _state = state;
        _logger = logger;
    }

    public Task SetSnapshotAsync(PlayerSnapshot snapshot)
    {
        if (snapshot.CharId != (int)this.GetPrimaryKeyLong())
        {
            throw new ArgumentException(
                $"Snapshot.CharId ({snapshot.CharId}) doesn't match grain key ({this.GetPrimaryKeyLong()}).",
                nameof(snapshot));
        }
        _state.State = snapshot;
        return _state.WriteStateAsync();
    }

    public Task<PlayerSnapshot> GetSnapshotAsync() => Task.FromResult(_state.State ?? new PlayerSnapshot { CharId = (int)this.GetPrimaryKeyLong() });

    public async Task EnterMapAsync(short mapId, byte channel, byte worldId)
    {
        var newKey = MapGrainKey.For(worldId, channel, mapId);
        if (_currentMapKey == newKey)
        {
            return; // idempotent — already in this map
        }

        // Leave the previous map first.
        if (_currentMap is not null)
        {
            await _currentMap.LeaveAsync((int)this.GetPrimaryKeyLong());
        }

        _currentMap = GrainFactory.GetGrain<IMapGrain>(newKey);
        _currentMapKey = newKey;
        await _currentMap.JoinAsync((int)this.GetPrimaryKeyLong());

        // Mirror map+channel onto the snapshot so subsequent reads see it.
        _state.State = _state.State with { MapId = mapId, Channel = channel, WorldId = worldId };
        await _state.WriteStateAsync();

        _logger.LogInformation(
            "PlayerGrain {CharId} entered map {Key}",
            this.GetPrimaryKeyLong(), newKey);
    }

    public async Task LeaveMapAsync()
    {
        if (_currentMap is null)
        {
            return;
        }
        await _currentMap.LeaveAsync((int)this.GetPrimaryKeyLong());
        _logger.LogInformation(
            "PlayerGrain {CharId} left map {Key}",
            this.GetPrimaryKeyLong(), _currentMapKey);
        _currentMap = null;
        _currentMapKey = null;
    }

    public Task MoveAsync(float x, float y, float z, short dir)
    {
        // Update grain state; AOI broadcast comes when CS_MOVE handler is wired.
        _state.State = _state.State with { PosX = x, PosY = y, PosZ = z, Dir = dir };
        // No WriteStateAsync — position updates would dominate IO. State is held
        // in memory; we persist on map-leave / explicit checkpoint instead.
        return Task.CompletedTask;
    }
}
