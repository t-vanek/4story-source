namespace FourStory.Map.Grains;

/// <summary>
/// Authoritative actor for a single (worldId, channelId, mapId) tuple. Owns the
/// roster of <see cref="IPlayerGrain"/>s currently inside and provides AOI
/// queries that the gameplay handlers (movement, chat, attack) will use to
/// broadcast updates to visible players.
///
/// Replaces the legacy <c>CTMap</c> (Server/TMapSvr/TMap.cpp, 2111 LOC) +
/// <c>CTCell</c> (Server/TMapSvr/TCell.cpp, 671 LOC). The legacy spatial index
/// is a 2D grid of cells; the C# port starts with a naive list (O(N) AOI scan)
/// and switches to a spatial index once population justifies it. With ~50
/// players per channel the naive scan is fine.
///
/// Key: composite string in the form <c>"{worldId}/{channelId}/{mapId}"</c>
/// (Orleans <see cref="Orleans.IGrainWithStringKey"/> for safe deterministic
/// activation across the cluster).
/// </summary>
public interface IMapGrain : Orleans.IGrainWithStringKey
{
    /// <summary>
    /// Add a player to this map's roster. Triggers AOI broadcast to nearby
    /// players (deferred — currently a no-op).
    /// </summary>
    Task JoinAsync(int charId);

    /// <summary>Remove a player. Called on disconnect or zone change.</summary>
    Task LeaveAsync(int charId);

    /// <summary>Returns char IDs visible to <paramref name="charId"/>'s position.</summary>
    Task<IReadOnlyList<int>> GetNeighborsAsync(int charId);

    /// <summary>Total active players. Used by lobby's GROUPLIST aggregation.</summary>
    Task<int> GetPlayerCountAsync();
}

/// <summary>
/// Helper for constructing the canonical <see cref="IMapGrain"/> key.
/// </summary>
public static class MapGrainKey
{
    public static string For(byte worldId, byte channelId, short mapId) =>
        System.FormattableString.Invariant($"{worldId}/{channelId}/{mapId}");

    public static (byte worldId, byte channelId, short mapId) Parse(string key)
    {
        var parts = key.Split('/');
        var inv = System.Globalization.CultureInfo.InvariantCulture;
        return (byte.Parse(parts[0], inv), byte.Parse(parts[1], inv), short.Parse(parts[2], inv));
    }
}
