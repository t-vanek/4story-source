namespace FourStory.Login.Auth;

public interface IAuthService
{
    Task<AuthOutcome> AuthenticateAsync(
        string userId,
        string password,
        string loginIp,
        byte ipCheckFlag,
        CancellationToken ct);
}
