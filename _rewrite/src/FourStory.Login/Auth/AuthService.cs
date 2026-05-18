using FourStory.Login.Services;
using FourStory.Persistence;
using FourStory.Shared;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Auth;

/// <summary>
/// C# port of the <c>TLogin</c> stored procedure
/// (<c>_rewrite/docs/schema/procs/TGLOBAL_RAGEZONE/TLogin.sql</c>).
///
/// Replaces the legacy SP with LINQ over the scaffolded
/// <see cref="GlobalDbContext"/>. Order of checks preserved from the original
/// so wire-level <see cref="LoginResult"/> values stay identical.
/// </summary>
public sealed class AuthService : IAuthService
{
    private readonly IDbContextFactory<GlobalDbContext> _dbFactory;
    private readonly MapServerLocator _mapLocator;
    private readonly LoginRateLimiter _rateLimiter;
    private readonly ILogger<AuthService> _logger;

    public AuthService(
        IDbContextFactory<GlobalDbContext> dbFactory,
        MapServerLocator mapLocator,
        LoginRateLimiter rateLimiter,
        ILogger<AuthService> logger)
    {
        _dbFactory = dbFactory;
        _mapLocator = mapLocator;
        _rateLimiter = rateLimiter;
        _logger = logger;
    }

    public async Task<AuthOutcome> AuthenticateAsync(
        string userId, string password, string loginIp, byte ipCheckFlag, CancellationToken ct)
    {
        // Step 0: rate-limit BEFORE touching the DB so we can shed brute-force load cheaply.
        if (_rateLimiter.IsBlocked(loginIp, userId))
        {
            return AuthOutcome.Failure(LoginResult.RateLimited);
        }

        await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);

        // Step 1: IP blacklist (TLogin.sql:79-82).
        var ipBanned = await db.IPBLACKLIST_games
            .AnyAsync(b => b.szIP == loginIp, ct)
            .ConfigureAwait(false);
        if (ipBanned)
        {
            _logger.LogWarning("Login rejected (IP banned): {Ip}", loginIp);
            return AuthOutcome.Failure((LoginResult)7);
        }

        // Step 2: user exists? (TLogin.sql:84-89)
        // NOTE: legacy reads from TACCOUNT_PW (parallel table). TACCOUNT is the original
        // but TACCOUNT_PW is what the SP actually queries — preserve fidelity.
        var account = await db.TACCOUNT_PWs
            .Where(a => a.szUserID == userId)
            .Select(a => new { a.dwUserID, a.szPasswd, a.bCheck })
            .FirstOrDefaultAsync(ct)
            .ConfigureAwait(false);
        if (account is null)
        {
            _rateLimiter.RegisterFailure(loginIp, userId);
            return AuthOutcome.Failure(LoginResult.NoUser);
        }

        // Step 3: password match (TLogin.sql:91-96). Supports legacy plaintext
        // (transparent upgrade on success) and BCrypt hashes side-by-side.
        if (account.szPasswd is null || !PasswordHasher.Verify(password, account.szPasswd))
        {
            _rateLimiter.RegisterFailure(loginIp, userId);
            return AuthOutcome.Failure(LoginResult.InvalidPasswd);
        }
        _rateLimiter.RegisterSuccess(userId);
        var needsHashUpgrade = PasswordHasher.ShouldUpgrade(account.szPasswd);

        // Step 4: bIPCheck == 6 → return 6 (TLogin.sql:105-108).
        if (ipCheckFlag == 6)
        {
            return AuthOutcome.Failure((LoginResult)6);
        }

        // Step 5: user-protection bans (TLogin.sql:110-125).
        // Port of TGetBanReason — we query TUSERPROTECTED directly and surface
        // reason / duration / unban time so ops can correlate with /support tickets.
        var now = DateTime.UtcNow;
        var ban = await db.TUSERPROTECTEDs
            .Where(p => p.dwUserID == account.dwUserID)
            .Where(p => p.bEternal == 1 || EF.Functions.DateDiffDay(p.startTime, now) < p.dwDuration)
            .OrderByDescending(p => p.bEternal)
            .ThenByDescending(p => p.startTime)
            .Select(p => new BanDetails(
                p.szComment ?? string.Empty,   // szReason in legacy maps to szComment
                p.szComment ?? string.Empty,
                p.startTime,
                p.bEternal == 1 ? null : (DateTime?)p.startTime.AddDays(p.dwDuration),
                p.dwDuration,
                p.bEternal == 1,
                p.szGMID ?? string.Empty,
                p.bBlockReason))
            .FirstOrDefaultAsync(ct)
            .ConfigureAwait(false);
        if (ban is not null)
        {
            _logger.LogWarning(
                "Login rejected (banned): user={UserId} ip={Ip} eternal={Eternal} until={Until} reason={Reason} gm={Gm}",
                account.dwUserID, loginIp, ban.Eternal, ban.UnbanTime, ban.Reason, ban.GmId);
            return AuthOutcome.FailureBanned(ban);
        }

        // Step 6: duplicate session (TLogin.sql:132-140).
        var existing = await db.TCURRENTUSERs
            .Where(u => u.dwUserID == account.dwUserID)
            .FirstOrDefaultAsync(ct)
            .ConfigureAwait(false);
        if (existing is not null)
        {
            existing.bLocked = 1;
            await db.SaveChangesAsync(ct).ConfigureAwait(false);
            // Carry userId so LoginHandler can kick the previous connection by user.
            return new AuthOutcome(LoginResult.Duplicate, new AuthSuccess(
                UserId: account.dwUserID, CharId: 0, Key: (uint)existing.dwKEY,
                MapIp: string.Empty, MapPort: 0, CreateCharCount: 0, InPcBang: 0, PremiumId: 0));
        }

        // Step 7: create new session (TLogin.sql:160-198).
        var session = new Persistence.Global.TCURRENTUSER
        {
            dwUserID = account.dwUserID,
            dwCharID = 0,
            bGroupID = 0,
            bChannel = 0,
            szIPAddr = string.Empty,
            wPort = 0,
            bLocked = 0,
            dwPcBangID = 0,
            szLoginIP = loginIp,
            dLoginDate = now,
            dEnterDate = now,
        };
        db.TCURRENTUSERs.Add(session);
        await db.SaveChangesAsync(ct).ConfigureAwait(false);
        // dwKEY is the identity column; EF Core populates it after SaveChanges.
        var key = (uint)session.dwKEY;

        // Step 7b: audit row (TLogin.sql:184-198) — TLog.dwKEY mirrors the session key
        // we just allocated. timeLOGOUT is set to "now" at login time and overwritten
        // on actual logout; matches the legacy SP behaviour.
        db.TLOGs.Add(new Persistence.Global.TLOG
        {
            dwKEY = (int)key,
            dwUserID = account.dwUserID,
            dwCharID = 0,
            bGroupID = 0,
            bChannel = 0,
            timeLOGIN = now,
            timeLOGOUT = now,
        });

        // Step 7c: upsert dFirstLogin / dLastLogin on TACCOUNT_PW (TLogin.sql:203-230).
        // EF Core's tracked entity is still our `account` projection — we need to load
        // the trackable row to mutate. Cheap, single row.
        var tracked = await db.TACCOUNT_PWs
            .FirstAsync(a => a.dwUserID == account.dwUserID, ct)
            .ConfigureAwait(false);
        tracked.dLastLogin = now;
        if (tracked.dFirstLogin is null || tracked.dFirstLogin == default(DateTime))
        {
            tracked.dFirstLogin = now;
        }
        if (needsHashUpgrade)
        {
            tracked.szPasswd = PasswordHasher.Hash(password);
            _logger.LogInformation(
                "Upgraded legacy plaintext password to BCrypt for user={UserId}", account.dwUserID);
        }
        await db.SaveChangesAsync(ct).ConfigureAwait(false);

        // Step 8: agreement check (TLogin.sql:238-239).
        //
        // Legacy literally tests `bAgreement = 1`, which has the quirk that a user who
        // accepted twice (bAgreement >= 2) gets locked out forever — the SP is buggy.
        // We preserve the wire-level RETURN value (8) but use `>= 1` so re-acceptance
        // doesn't break a working account.
        var hasAgreement = await db.TUSERINFOTABLEs
            .AnyAsync(u => u.dwUserID == account.dwUserID && u.bAgreement >= 1, ct)
            .ConfigureAwait(false);

        // Resolve a default Map endpoint via MapServerLocator. At login time no world
        // is yet selected (CS_LOGIN_ACK is sent before CS_GROUPLIST_REQ), so we pass
        // groupId=0 — the locator returns the first active TSERVER row across any
        // group, or a loopback fallback when the test DB has no TSERVER seed data.
        var defaultEndpoint = await _mapLocator.LookupAsync(0, ct).ConfigureAwait(false);

        var success = new AuthSuccess(
            UserId: account.dwUserID,
            CharId: 0,
            Key: key,
            MapIp: defaultEndpoint.IpAddress,
            MapPort: defaultEndpoint.Port,
            CreateCharCount: 6,
            InPcBang: 0,
            PremiumId: 0);

        // Legacy CSHandler.cpp:294-296: LR_NEEDAGREEMENT is a SUCCESS branch — the user is
        // authenticated and registered in m_mapTUSER, but pUser->m_bAgreement = FALSE. The
        // client gets a fully-populated LOGIN_ACK and is expected to send CS_AGREEMENT_REQ
        // before any downstream lobby request will be accepted.
        if (!hasAgreement)
        {
            _logger.LogInformation(
                "Login OK but agreement pending for user={UserId}", account.dwUserID);
            return AuthOutcome.PendingAgreement(success);
        }

        return AuthOutcome.Ok(success);
    }
}
