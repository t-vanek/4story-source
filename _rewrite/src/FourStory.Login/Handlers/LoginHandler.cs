using System.Net;
using System.Security.Cryptography;
using FourStory.Login.Auth;
using FourStory.Login.Services;
using FourStory.Protocol;
using FourStory.Protocol.Session;
using FourStory.Shared;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Handlers;

/// <summary>
/// Wire handler for <see cref="MessageId.CS_LOGIN_REQ"/> (0x1988) → <see cref="MessageId.CS_LOGIN_ACK"/> (0x1989).
/// Mirrors <c>Server/TLoginSvr/CSHandler.cpp::OnCS_LOGIN_REQ</c>.
/// </summary>
public sealed class LoginHandler
{
    private readonly IAuthService _auth;
    private readonly ConnectionRegistry? _registry;
    private readonly ILogger<LoginHandler> _logger;

    public LoginHandler(IAuthService auth, ILogger<LoginHandler> logger, ConnectionRegistry? registry = null)
    {
        _auth = auth;
        _registry = registry;
        _logger = logger;
    }

    public async ValueTask HandleAsync(
        ClientConnection conn,
        ReadOnlyMemory<byte> body,
        CancellationToken ct)
    {
        // === Wire layout (CSHandler.cpp:148-164) ===
        // WORD   wVersion
        // STRING Zombie3
        // STRING strPasswd
        // STRING Zombie1
        // STRING Zombie2
        // STRING strUserID
        // INT64  dlCheck           — client exec-file integrity hash (ignored when server has no exec file)
        // INT64  llChecksum_recv   — handshake checksum, validated by LoginChecksum.Compute(version)
        // [BYTE  bChanneling]      — JP nation only; we don't read it
        var reader = new PacketReader(body.Span);
        var version = reader.ReadUInt16();

        if (version != ProtocolConstants.Version)
        {
            _logger.LogWarning(
                "Login version mismatch (ip={Ip}): got 0x{Got:X4}, expected 0x{Expected:X4}",
                conn.RemoteAddress, version, ProtocolConstants.Version);
            await SendAckAsync(conn, LoginResult.VersionMismatch, null, ct).ConfigureAwait(false);
            return;
        }

        // Consume the zombies and bracketing strings exactly as legacy does.
        var zombie3 = reader.ReadString();
        var password = reader.ReadString();
        var zombie1 = reader.ReadString();
        var zombie2 = reader.ReadString();
        var userId = reader.ReadString();
        var dlCheck = reader.ReadInt64();
        var llChecksumRecv = reader.ReadInt64();

        // Validate the handshake checksum. Legacy returns EC_SESSION_INVALIDCHAR (no ACK)
        // on mismatch; we mirror that — a wrong-version client is hostile, no need to be polite.
        var expectedChecksum = LoginChecksum.Compute(version);
        if (expectedChecksum != llChecksumRecv)
        {
            _logger.LogWarning(
                "Login checksum mismatch (ip={Ip}, version=0x{V:X4}): got 0x{Got:X16}, expected 0x{Exp:X16}",
                conn.RemoteAddress, version, llChecksumRecv, expectedChecksum);
            return;
        }

        // Max id/password length cap (CSHandler.cpp:177-183). Legacy returns LR_INTERNAL.
        if (userId.Length > ProtocolConstants.MaxNameLength ||
            password.Length > ProtocolConstants.MaxNameLength)
        {
            await SendAckAsync(conn, LoginResult.InternalError, null, ct).ConfigureAwait(false);
            return;
        }

        // Generate the per-session 64-bit key BEFORE auth (legacy CSHandler.cpp:231).
        // We use a cryptographically secure RNG; legacy uses two TRand() calls.
        Span<byte> rng = stackalloc byte[8];
        RandomNumberGenerator.Fill(rng);
        conn.State.DlCheckKey = BitConverter.ToInt64(rng);

        var outcome = await _auth.AuthenticateAsync(
            userId, password, conn.RemoteAddress, ipCheckFlag: 0, ct).ConfigureAwait(false);

        _logger.LogInformation(
            "Login {Outcome} for {UserId} from {Ip}",
            outcome.Result, userId, conn.RemoteAddress);

        // Legacy treats LR_NEEDAGREEMENT as a successful login that just hasn't accepted the
        // EULA yet — it sends the same ACK as LR_SUCCESS, with the user authenticated in
        // memory but `m_bAgreement = FALSE`. Downstream handlers gate on m_bAgreement.
        var wireResult = outcome.Result == LoginResult.AgreementRequired
            ? LoginResult.Success
            : outcome.Result;
        await SendAckAsync(conn, wireResult, outcome.Success, ct).ConfigureAwait(false);

        // Legacy CSHandler.cpp:271-289: on LR_DUPLICATE, locate the previous live connection
        // for this userId and force it closed. The OLD session keeps TCURRENTUSER (m_bLogout
        // = FALSE) so MapSvr can still validate any in-flight handoff key; we just drop the
        // TCP socket. The NEW (duplicate) connection got the Duplicate ACK already and exits.
        if (outcome.Result == LoginResult.Duplicate && outcome.Success is { } dup && _registry is not null)
        {
            var old = _registry.FindByUserId(dup.UserId);
            if (old is not null && !ReferenceEquals(old, conn))
            {
                _logger.LogInformation(
                    "Duplicate login for user={UserId} from {NewIp} — kicking old session at {OldIp}",
                    dup.UserId, conn.RemoteAddress, old.RemoteAddress);
                // Preserve the old session's TCURRENTUSER row; cleanup happens when MapSvr
                // finalizes or the stale-session sweeper runs.
                old.State.HandoffToMap = true;
                old.Session.Stop();
            }
            return;
        }

        if (outcome.IsAuthenticatedOutcome && outcome.Success is { } s)
        {
            conn.State.UserId = s.UserId;
            conn.State.Key = s.Key;
            conn.State.InPcBang = s.InPcBang != 0;
            // Only flip the agreement gate when login was fully successful — pending-agreement
            // sessions must wait for CS_AGREEMENT_REQ before downstream handlers will respond.
            conn.State.AgreementAccepted = outcome.Result == LoginResult.Success;
            conn.Session.Codec.CryptEnabled = true;
        }
    }

    /// <summary>
    /// Writes the 41-byte CS_LOGIN_ACK payload exactly as <c>CSSender.cpp::SendCS_LOGIN_ACK</c>.
    /// Field order: bResult, dwUserID, dwCharID, dwKEY, dwIPAddr, wPort, bCreateCnt,
    /// bInPcBang, dwPremium, dCurTime, dlCheckKey.
    /// </summary>
    private static async ValueTask SendAckAsync(
        ClientConnection conn,
        LoginResult result,
        AuthSuccess? success,
        CancellationToken ct)
    {
        const int payloadSize = 1 + 4 + 4 + 4 + 4 + 2 + 1 + 1 + 4 + 8 + 8; // = 41
        Span<byte> payload = stackalloc byte[payloadSize];
        var writer = new PacketWriter(payload);

        writer.WriteByte((byte)result);
        writer.WriteUInt32((uint)(success?.UserId ?? 0));
        writer.WriteUInt32((uint)(success?.CharId ?? 0));
        writer.WriteUInt32(success?.Key ?? 0);
        writer.WriteUInt32(success is null ? 0 : IpToDword(success.MapIp));
        writer.WriteUInt16(success?.MapPort ?? 0);
        writer.WriteByte(success?.CreateCharCount ?? 0);
        writer.WriteByte(success?.InPcBang ?? 0);
        writer.WriteUInt32(success?.PremiumId ?? 0);
        writer.WriteInt64(DateTimeOffset.UtcNow.ToUnixTimeSeconds());
        writer.WriteInt64(conn.State.DlCheckKey);

        await conn.Session.SendAsync(MessageId.CS_LOGIN_ACK, writer.WrittenSpan, ct).ConfigureAwait(false);
    }

    private static uint IpToDword(string ipString)
    {
        if (!IPAddress.TryParse(ipString, out var ip))
        {
            return 0;
        }
        // C++ used network byte order (htonl). On x86 LE, the bytes are stored as the IP
        // address octets in order. GetAddressBytes returns big-endian; reinterpret as little
        // endian uint and the C++ side reads it back with the same layout.
        var bytes = ip.GetAddressBytes();
        if (bytes.Length != 4)
        {
            return 0;
        }
        return (uint)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
    }
}
