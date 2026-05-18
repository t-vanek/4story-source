using FourStory.Persistence;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Auth;

/// <summary>
/// Removes a user's TCURRENTUSER row + stamps the matching TLog.timeLOGOUT.
/// Wired into <see cref="Handlers.ClientConnection.DisposeAsync"/> so an abrupt
/// disconnect doesn't leave a stale session row that would block the user's
/// next login via the duplicate-session check in <see cref="AuthService"/>.
///
/// Equivalent to TLogout SP in the legacy server, plus the C++ SessionClose
/// path which also clears m_mapTUSER.
/// </summary>
public sealed class SessionTerminator
{
    private readonly IDbContextFactory<GlobalDbContext> _dbFactory;
    private readonly ILogger<SessionTerminator> _logger;

    public SessionTerminator(IDbContextFactory<GlobalDbContext> dbFactory, ILogger<SessionTerminator> logger)
    {
        _dbFactory = dbFactory;
        _logger = logger;
    }

    /// <summary>
    /// Best-effort cleanup. Swallows DB errors so a session-close path can't take down the
    /// reader loop — by the time we're here the session is already gone.
    /// </summary>
    public async Task TerminateAsync(int userId, uint sessionKey, CancellationToken ct)
    {
        try
        {
            await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
            var now = DateTime.UtcNow;

            var rows = await db.TCURRENTUSERs
                .Where(s => s.dwUserID == userId)
                .ExecuteDeleteAsync(ct).ConfigureAwait(false);

            // Update the audit row's timeLOGOUT to reflect actual disconnect time.
            await db.TLOGs
                .Where(l => l.dwKEY == (int)sessionKey)
                .ExecuteUpdateAsync(s => s.SetProperty(l => l.timeLOGOUT, now), ct)
                .ConfigureAwait(false);

            _logger.LogInformation(
                "Session terminated: user={UserId} key=0x{Key:X8} rows={Rows}",
                userId, sessionKey, rows);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex,
                "SessionTerminator failed for user={UserId} key=0x{Key:X8} (continuing)",
                userId, sessionKey);
        }
    }
}
