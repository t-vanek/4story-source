using FourStory.Persistence;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Services;

/// <summary>
/// Periodically reaps TCURRENTUSER rows that no longer correspond to a live session.
///
/// Two cohorts are cleaned:
///   1. <c>bLocked = 1</c> rows older than 5 minutes — TLogin marks the previous
///      session as locked when a duplicate login arrives; if the original client never
///      reconnects to consume that mark, the row would persist forever.
///   2. Any TCURRENTUSER row older than the absolute timeout (default 24h) — these
///      indicate a process crash where <see cref="Auth.SessionTerminator"/> never ran.
///
/// Without this sweeper, the duplicate-session check in TLogin would lock out users
/// whose previous session crashed before <c>TerminateAsync</c> ran.
/// </summary>
public sealed class StaleSessionCleaner : BackgroundService
{
    public static readonly TimeSpan LockedGrace = TimeSpan.FromMinutes(5);
    public static readonly TimeSpan AbsoluteTimeout = TimeSpan.FromHours(24);
    private static readonly TimeSpan SweepInterval = TimeSpan.FromMinutes(5);

    private readonly IDbContextFactory<GlobalDbContext> _dbFactory;
    private readonly ILogger<StaleSessionCleaner> _logger;
    private readonly TimeProvider _clock;

    public StaleSessionCleaner(
        IDbContextFactory<GlobalDbContext> dbFactory,
        ILogger<StaleSessionCleaner> logger,
        TimeProvider? clock = null)
    {
        _dbFactory = dbFactory;
        _logger = logger;
        _clock = clock ?? TimeProvider.System;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var timer = new PeriodicTimer(SweepInterval);
        // Run once at startup so a crash-restart clears immediately, without waiting a full interval.
        try { await SweepOnceAsync(stoppingToken).ConfigureAwait(false); } catch (Exception ex) { _logger.LogError(ex, "Initial stale-session sweep failed"); }

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                if (!await timer.WaitForNextTickAsync(stoppingToken).ConfigureAwait(false))
                {
                    return;
                }
                await SweepOnceAsync(stoppingToken).ConfigureAwait(false);
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                return;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Stale session sweep failed");
            }
        }
    }

    internal async Task<(int Locked, int Old)> SweepOnceAsync(CancellationToken ct)
    {
        await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
        var now = _clock.GetUtcNow().UtcDateTime;
        var lockedCutoff = now - LockedGrace;
        var absCutoff = now - AbsoluteTimeout;

        var lockedDeleted = await db.TCURRENTUSERs
            .Where(u => u.bLocked == 1 && u.dLoginDate < lockedCutoff)
            .ExecuteDeleteAsync(ct).ConfigureAwait(false);

        var oldDeleted = await db.TCURRENTUSERs
            .Where(u => u.dLoginDate < absCutoff)
            .ExecuteDeleteAsync(ct).ConfigureAwait(false);

        if (lockedDeleted > 0 || oldDeleted > 0)
        {
            _logger.LogInformation(
                "TCURRENTUSER sweep: removed {Locked} locked + {Old} stale row(s)",
                lockedDeleted, oldDeleted);
        }
        return (lockedDeleted, oldDeleted);
    }
}
