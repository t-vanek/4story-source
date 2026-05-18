using FourStory.Shared;

namespace FourStory.Login.Auth;

/// <summary>
/// Successful authentication outcome. Mirrors the C++
/// <c>CSPLogin</c> output-parameter set used by <c>CS_LOGIN_ACK</c>.
/// </summary>
public sealed record AuthSuccess(
    int UserId,
    int CharId,
    uint Key,
    string MapIp,
    ushort MapPort,
    byte CreateCharCount,
    byte InPcBang,
    uint PremiumId);

/// <summary>
/// Ban details surfaced when <see cref="LoginResult.Banned"/> is returned.
/// Mirrors the legacy <c>TGetBanReason</c> SP output (<c>szReason, dwDuration,
/// dUnbanTime, bEternal</c>) plus the block reason / GM comment for ops triage.
/// </summary>
public sealed record BanDetails(
    string Reason,
    string Comment,
    DateTime StartTime,
    DateTime? UnbanTime,
    int DurationDays,
    bool Eternal,
    string GmId,
    byte BlockReason);

/// <summary>
/// Discriminated outcome of <see cref="IAuthService.AuthenticateAsync"/>.
/// </summary>
public readonly record struct AuthOutcome(LoginResult Result, AuthSuccess? Success, BanDetails? Ban = null)
{
    public static AuthOutcome Failure(LoginResult result) => new(result, null);
    public static AuthOutcome FailureBanned(BanDetails ban) => new(LoginResult.Banned, null, ban);
    public static AuthOutcome Ok(AuthSuccess s) => new(LoginResult.Success, s);
    /// <summary>
    /// Authenticated but TUSERINFOTABLE.bAgreement = 0. Carries the full <see cref="AuthSuccess"/>
    /// so the wire ACK looks like a successful login; the in-memory <c>State.AgreementAccepted</c>
    /// stays false until CS_AGREEMENT_REQ is received.
    /// </summary>
    public static AuthOutcome PendingAgreement(AuthSuccess s) => new(LoginResult.AgreementRequired, s);
    public bool IsSuccess => Result == LoginResult.Success && Success is not null;
    public bool IsAuthenticatedOutcome => Success is not null
        && (Result == LoginResult.Success || Result == LoginResult.AgreementRequired);
}
