using System.Collections.Concurrent;

namespace FourStory.Map.Network;

/// <summary>
/// Thread-safe lookup from <c>dwCharID</c> to the live <see cref="MapConnection"/>
/// that owns the character session. Inbound SS packets from World (sent through
/// the single <see cref="WorldClient"/> link) carry a <c>dwCharID</c> only — we
/// use this map to find the right per-client session to populate.
///
/// C++ parallel: <c>CTMapSvrModule::FindPlayer(DWORD dwID, DWORD dwKEY)</c>
/// (see <c>Server/TMapSvr/SSHandler.cpp:3083</c>) which scans the global
/// <c>m_mapPlayer</c>. We key by char id (the key/userId are stored on the
/// connection state for verification rather than being part of the lookup
/// hash, mirroring how C++ uses <c>dwID</c> as the primary key with
/// <c>dwKEY</c> as a secondary correctness check).
///
/// Lifetime: a connection registers itself once <c>CS_CONNECT_REQ</c> passes
/// (the point at which both <c>dwCharID</c> and <c>dwKEY</c> are known and
/// stamped onto <see cref="MapSessionState"/>); it unregisters from
/// <see cref="MapConnection.DisposeAsync"/>.
/// </summary>
public sealed class MapConnectionRegistry
{
    private readonly ConcurrentDictionary<int, MapConnection> _byCharId = new();

    public void Register(int charId, MapConnection conn) => _byCharId[charId] = conn;

    /// <summary>Removes only when the stored entry matches <paramref name="conn"/>,
    /// so a stale unregister from a recycled connection can't clobber a fresh one.</summary>
    public void Unregister(int charId, MapConnection conn)
    {
        // ConcurrentDictionary has no atomic compare-and-remove on value reference,
        // so fall back to a TryGet + compare + TryRemove. The window is tiny and
        // worst case we leave a stale entry that the next Register overwrites.
        if (_byCharId.TryGetValue(charId, out var existing) && ReferenceEquals(existing, conn))
        {
            _byCharId.TryRemove(charId, out _);
        }
    }

    public bool TryGet(int charId, out MapConnection conn) => _byCharId.TryGetValue(charId, out conn!);
}
