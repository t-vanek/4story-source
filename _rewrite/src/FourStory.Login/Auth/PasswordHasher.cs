namespace FourStory.Login.Auth;

/// <summary>
/// Password verification + transparent upgrade path from the legacy plaintext
/// <c>TACCOUNT_PW.szPasswd</c> column to BCrypt hashes.
///
/// Strategy:
/// <list type="number">
/// <item>If the stored value starts with a BCrypt prefix (<c>$2a$</c>, <c>$2b$</c>,
/// <c>$2y$</c>), verify with BCrypt and return.</item>
/// <item>Otherwise treat the stored value as a legacy plaintext password.
/// Constant-time compare; on match, the caller is expected to call
/// <see cref="Hash"/> and rewrite the column — the next login then takes the
/// BCrypt path with no user-visible difference.</item>
/// </list>
///
/// We don't drop legacy plaintext support outright because the seeded DB ships
/// plaintext (e.g., the <c>testuser</c>/<c>testpass</c> row). The migration is
/// natural — every login upgrades one row.
/// </summary>
public sealed class PasswordHasher
{
    private const int WorkFactor = 11; // ~250 ms on modern hardware, OWASP 2023+ floor

    public static string Hash(string plaintext) => BCrypt.Net.BCrypt.HashPassword(plaintext, WorkFactor);

    public static bool Verify(string plaintextAttempt, string stored)
    {
        if (string.IsNullOrEmpty(stored))
        {
            return false;
        }

        if (LooksLikeBcrypt(stored))
        {
            try
            {
                return BCrypt.Net.BCrypt.Verify(plaintextAttempt, stored);
            }
            catch (BCrypt.Net.SaltParseException)
            {
                // Malformed hash — treat as auth failure rather than fall back to plaintext;
                // refusing is the safe direction.
                return false;
            }
        }

        // Legacy plaintext column — constant-time compare to avoid timing leaks.
        return ConstantTimeEquals(plaintextAttempt, stored);
    }

    /// <summary>
    /// Returns <c>true</c> if the stored value is a legacy plaintext password that
    /// matched on this login and should be upgraded to a BCrypt hash on the
    /// next <c>SaveChangesAsync</c>.
    /// </summary>
    public static bool ShouldUpgrade(string stored) => !LooksLikeBcrypt(stored);

    private static bool LooksLikeBcrypt(string s) =>
        s.Length >= 4
            && s[0] == '$'
            && s[1] == '2'
            && (s[2] is 'a' or 'b' or 'y')
            && s[3] == '$';

    private static bool ConstantTimeEquals(string a, string b)
    {
        if (a.Length != b.Length)
        {
            return false;
        }
        var diff = 0;
        for (var i = 0; i < a.Length; i++)
        {
            diff |= a[i] ^ b[i];
        }
        return diff == 0;
    }
}
