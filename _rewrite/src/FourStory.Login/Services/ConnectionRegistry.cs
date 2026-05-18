using System.Collections.Concurrent;
using FourStory.Login.Handlers;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Services;

/// <summary>
/// Tracks every accepted <see cref="ClientConnection"/> so background jobs
/// (idle-session monitor, graceful-shutdown drain, ops introspection) can
/// iterate active sessions without each component owning its own list.
/// </summary>
public sealed class ConnectionRegistry
{
    private readonly ConcurrentDictionary<ClientConnection, byte> _connections = new();

    public void Add(ClientConnection conn) => _connections.TryAdd(conn, 0);
    public void Remove(ClientConnection conn) => _connections.TryRemove(conn, out _);
    public int Count => _connections.Count;
    public IEnumerable<ClientConnection> Snapshot() => _connections.Keys.ToArray();

    /// <summary>
    /// Finds the live connection authenticated as <paramref name="userId"/>, if any.
    /// Used by duplicate-session enforcement: legacy <c>OnCS_LOGIN_REQ</c> looks the user
    /// up in <c>m_mapTUSER</c> and forces the old TCP close when a second login arrives.
    /// </summary>
    public ClientConnection? FindByUserId(int userId)
    {
        foreach (var c in _connections.Keys)
        {
            if (c.State.UserId == userId)
            {
                return c;
            }
        }
        return null;
    }
}

/// <summary>
/// Periodically evicts <see cref="ClientConnection"/>s whose <c>PacketSession.LastActivityUtc</c>
/// is older than <see cref="IdleTimeout"/>. Defends against NAT timeouts and half-open
/// crashes — without this, dead TCP sockets accumulate until the kernel keepalive (~2h)
/// or until the next write attempt fails.
/// </summary>
public sealed class IdleSessionMonitor : BackgroundService
{
    public static readonly TimeSpan IdleTimeout = TimeSpan.FromMinutes(10);
    private static readonly TimeSpan ScanInterval = TimeSpan.FromMinutes(1);

    private readonly ConnectionRegistry _registry;
    private readonly ILogger<IdleSessionMonitor> _logger;
    private readonly TimeProvider _clock;

    public IdleSessionMonitor(
        ConnectionRegistry registry,
        ILogger<IdleSessionMonitor> logger,
        TimeProvider? clock = null)
    {
        _registry = registry;
        _logger = logger;
        _clock = clock ?? TimeProvider.System;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var timer = new PeriodicTimer(ScanInterval);
        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                if (!await timer.WaitForNextTickAsync(stoppingToken).ConfigureAwait(false))
                {
                    return;
                }
                ScanOnce();
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                return;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Idle session monitor scan failed");
            }
        }
    }

    internal void ScanOnce()
    {
        var cutoff = _clock.GetUtcNow() - IdleTimeout;
        var evicted = 0;
        foreach (var conn in _registry.Snapshot())
        {
            if (conn.Session.LastActivityUtc < cutoff)
            {
                _logger.LogInformation(
                    "Evicting idle session ip={Ip} user={UserId} idleFor={IdleFor}",
                    conn.RemoteAddress, conn.State.UserId,
                    _clock.GetUtcNow() - conn.Session.LastActivityUtc);
                conn.Session.Stop();
                evicted++;
            }
        }
        if (evicted > 0)
        {
            _logger.LogInformation("Idle scan evicted {Count} session(s); {Remaining} remain", evicted, _registry.Count);
        }
    }
}
