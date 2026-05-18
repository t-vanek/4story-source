using FourStory.Login.Auth;
using FourStory.Protocol;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Handlers;

/// <summary>
/// Handles <c>CS_TERMINATE_REQ</c> (0x199x) — explicit clean logout request.
/// Equivalent to <c>CTLoginSvrModule::OnCS_TERMINATE_REQ</c>: server runs the
/// same cleanup as the disconnect path (delete TCURRENTUSER, stamp TLog.timeLOGOUT)
/// but also closes the TCP session.
///
/// Idempotent with <see cref="ClientConnection.DisposeAsync"/>'s automatic cleanup —
/// if the client requests TERMINATE and then immediately disconnects, the second
/// pass of SessionTerminator just no-ops.
/// </summary>
public sealed class TerminateHandler
{
    private readonly SessionTerminator _terminator;
    private readonly ILogger<TerminateHandler> _logger;

    public TerminateHandler(SessionTerminator terminator, ILogger<TerminateHandler> logger)
    {
        _terminator = terminator;
        _logger = logger;
    }

    public void Register(PacketDispatcher dispatcher)
    {
        dispatcher.Register(MessageId.CS_TERMINATE_REQ, OnTerminateAsync);
    }

    private async ValueTask OnTerminateAsync(ClientConnection conn, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        // Wire layout (CSHandler.cpp:1446-1456): DWORD dwKey, must equal 720809425 (0x2AF3A9D1).
        // Legacy returns EC_SESSION_INVALIDCHAR (no ACK, terminates session) on mismatch.
        if (body.Length < 4)
        {
            _logger.LogWarning(
                "CS_TERMINATE_REQ from {Ip} too short ({Bytes} bytes)",
                conn.RemoteAddress, body.Length);
            return;
        }
        var reader = new PacketReader(body.Span);
        var clientKey = reader.ReadUInt32();
        if (clientKey != ProtocolConstants.TerminateMagic)
        {
            _logger.LogWarning(
                "CS_TERMINATE_REQ magic mismatch from {Ip}: got 0x{Got:X8}, expected 0x{Exp:X8}",
                conn.RemoteAddress, clientKey, ProtocolConstants.TerminateMagic);
            return;
        }

        if (conn.State.UserId is int userId && conn.State.Key is uint key)
        {
            _logger.LogInformation(
                "CS_TERMINATE_REQ user={UserId} from {Ip} — clean logout",
                userId, conn.RemoteAddress);
            await _terminator.TerminateAsync(userId, key, ct).ConfigureAwait(false);
            conn.State.UserId = null;
            conn.State.Key = null;
        }
        else
        {
            _logger.LogInformation("CS_TERMINATE_REQ from {Ip} (no active auth)", conn.RemoteAddress);
        }
    }
}
