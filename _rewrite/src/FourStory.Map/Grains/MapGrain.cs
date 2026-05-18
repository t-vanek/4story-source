using Microsoft.Extensions.Logging;
using Orleans;

namespace FourStory.Map.Grains;

/// <summary>
/// Implementation of <see cref="IMapGrain"/>. Holds an in-memory roster of
/// active char IDs. Not persisted — map state is reconstructed from
/// <see cref="IPlayerGrain"/> activations.
///
/// <para><b>AOI strategy.</b></para>
/// Initial implementation: every player is a neighbour of every other player
/// (full roster broadcast). O(N²) on movement but trivial to reason about and
/// fine up to ~50 concurrent. When that's tight we plug in a real spatial
/// index (uniform grid mirroring TCell, or RBush).
/// </summary>
public sealed class MapGrain : Grain, IMapGrain
{
    private readonly ILogger<MapGrain> _logger;
    private readonly HashSet<int> _players = new();

    public MapGrain(ILogger<MapGrain> logger)
    {
        _logger = logger;
    }

    public Task JoinAsync(int charId)
    {
        if (_players.Add(charId))
        {
            _logger.LogInformation(
                "Map {Key}: char {CharId} joined (total {Count})",
                this.GetPrimaryKeyString(), charId, _players.Count);
        }
        return Task.CompletedTask;
    }

    public Task LeaveAsync(int charId)
    {
        if (_players.Remove(charId))
        {
            _logger.LogInformation(
                "Map {Key}: char {CharId} left (total {Count})",
                this.GetPrimaryKeyString(), charId, _players.Count);
        }
        return Task.CompletedTask;
    }

    public Task<IReadOnlyList<int>> GetNeighborsAsync(int charId)
    {
        // Naive AOI: everyone in the map is a neighbour, minus self.
        // Replace with spatial-index query when player density justifies it.
        var neighbors = _players.Where(id => id != charId).ToList();
        return Task.FromResult<IReadOnlyList<int>>(neighbors);
    }

    public Task<int> GetPlayerCountAsync() => Task.FromResult(_players.Count);
}
