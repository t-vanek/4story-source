using FourStory.Persistence;
using FourStory.Protocol;
using FourStory.Shared;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Services;

/// <summary>
/// Resolves the Map server endpoint (IP / port / serverID) for a given world group.
///
/// Mirrors the lookup that the legacy <c>TRoute</c> / <c>TLoadService</c> stored procedures
/// perform: <c>TSERVER</c> joined with <c>TIPADDR</c> (via <c>bMachineID</c>), filtered by
/// <c>bGroupID</c> and by <c>bType == SVRGRP_MAPSVR (4)</c>. The C++ caller is
/// <c>Server/TLoginSvr/CSHandler.cpp:1400-1406</c>, which passes
/// <c>m_bType = SVRGRP_MAPSVR</c> (defined in
/// <c>Lib/Own/TProtocol/include/CTProtocol.h:30</c>).
///
/// When no row exists for the group (typical for the integration-test DB), the locator
/// falls back to a loopback default so the wire-level handshake can still complete.
/// </summary>
public sealed class MapServerLocator
{
    private readonly IDbContextFactory<GlobalDbContext> _dbFactory;
    private readonly ILogger<MapServerLocator> _logger;

    public MapServerLocator(
        IDbContextFactory<GlobalDbContext> dbFactory,
        ILogger<MapServerLocator> logger)
    {
        _dbFactory = dbFactory;
        _logger = logger;
    }

    /// <summary>
    /// Look up the Map endpoint for a specific world group. Returns
    /// <see cref="MapServerEndpoint.Default"/> when no row matches.
    /// </summary>
    /// <param name="groupId">World ID (<c>TGROUP.bGroupID</c>). Pass 0 for the
    /// "any group" fallback used at login-time when no world is yet selected.</param>
    public Task<MapServerEndpoint> LookupAsync(byte groupId, CancellationToken ct)
        => LookupAsync(groupId, preferredServerId: null, ct);

    /// <summary>
    /// Look up a specific Map endpoint within a world group. When
    /// <paramref name="preferredServerId"/> is supplied the locator first tries to find
    /// a TSERVER row matching that <c>bServerID</c> (used by the BOW / BR routing override
    /// in <c>CS_START_REQ</c> — CSHandler.cpp:1377-1398); if no match, it falls back to
    /// the default lookup so the handshake still completes.
    /// </summary>
    public async Task<MapServerEndpoint> LookupAsync(byte groupId, byte? preferredServerId, CancellationToken ct)
    {
        await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);

        var query = db.TSERVERs
            .Where(s => s.bType == (byte)ServerType.MapSvr)
            .Join(db.TIPADDRs,
                s => s.bMachineID,
                ip => ip.bMachineID,
                (s, ip) => new { s.bGroupID, s.bServerID, s.wPort, ip.szIPAddr, ip.bActive });

        if (groupId != 0)
        {
            query = query.Where(x => x.bGroupID == groupId);
        }

        // Try preferred server id first (BOW=30, BR=50). If no row, fall through to default.
        if (preferredServerId is byte pref)
        {
            var preferred = await query
                .Where(x => x.bActive != 0 && x.bServerID == pref)
                .Select(x => new { x.bServerID, x.wPort, x.szIPAddr })
                .FirstOrDefaultAsync(ct).ConfigureAwait(false);
            if (preferred is not null && !string.IsNullOrEmpty(preferred.szIPAddr))
            {
                _logger.LogInformation(
                    "Routing to preferred ServerID={Sid} ({Ip}:{Port}) for group={Group}",
                    preferred.bServerID, preferred.szIPAddr, preferred.wPort, groupId);
                return new MapServerEndpoint(preferred.szIPAddr, (ushort)preferred.wPort, preferred.bServerID);
            }
            _logger.LogInformation(
                "No TSERVER row for preferred ServerID={Sid} in group={Group}; falling back to default",
                pref, groupId);
        }

        var row = await query
            .Where(x => x.bActive != 0)
            .OrderBy(x => x.bGroupID).ThenBy(x => x.bServerID)
            .Select(x => new { x.bServerID, x.wPort, x.szIPAddr })
            .FirstOrDefaultAsync(ct).ConfigureAwait(false);

        if (row is null || string.IsNullOrEmpty(row.szIPAddr))
        {
            _logger.LogInformation(
                "No TSERVER row for group={GroupId}; using default loopback endpoint {Ip}:{Port}.",
                groupId, MapServerEndpoint.Default.IpAddress, MapServerEndpoint.Default.Port);
            return MapServerEndpoint.Default;
        }

        return new MapServerEndpoint(
            IpAddress: row.szIPAddr,
            Port: (ushort)row.wPort,
            ServerId: row.bServerID);
    }
}

/// <summary>
/// Network endpoint plus identifier of the Map server the client should connect to.
/// </summary>
public sealed record MapServerEndpoint(string IpAddress, ushort Port, byte ServerId)
{
    /// <summary>Loopback fallback used when no TSERVER row exists (test / dev environments).</summary>
    public static MapServerEndpoint Default { get; } = new(
        IpAddress: "127.0.0.1",
        Port: ProtocolConstants.DefaultMapPort,
        ServerId: 1);
}
