namespace FourStory.Protocol;

/// <summary>
/// Computes the 64-bit checksum the legacy client embeds at the tail of
/// <c>CS_LOGIN_REQ</c>. The server validates it before touching the DB
/// (see <c>Server/TLoginSvr/CSHandler.cpp::OnCS_LOGIN_REQ</c> lines 186-202).
///
/// The algorithm is purely a function of the wire version, so it acts as a
/// "client knows the protocol version" handshake — it doesn't authenticate
/// the user, it just rejects clients built against the wrong protocol revision
/// before any password hashing work happens.
/// </summary>
public static class LoginChecksum
{
    private const long Key = unchecked((long)0x336c3aebf71a8b08UL);

    public static long Compute(ushort version)
    {
        long c = version * 2L - 500L;
        long dwIndex = c % 8;
        long dwBody = c / 8;
        // C++ uses DWORD loop var; an effectively unsigned loop bound is fine because
        // for any sensible wire version (0x2918) dwIndex is small and positive.
        for (long i = 0; i < dwIndex; i++)
        {
            c ^= dwBody;
            c += Key;
        }
        return c;
    }
}
