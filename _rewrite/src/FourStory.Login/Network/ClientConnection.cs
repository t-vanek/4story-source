using System.Diagnostics;
using FourStory.Login.Auth;
using FourStory.Protocol;
using FourStory.Protocol.Session;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Handlers;

/// <summary>
/// One per accepted TCP connection. Owns the <see cref="PacketSession"/>, dispatches
/// inbound packets to per-MessageId handlers, and exposes a Send API to handlers.
/// </summary>
public sealed class ClientConnection : IAsyncDisposable
{
    private readonly ILogger<ClientConnection> _logger;
    private readonly PacketDispatcher _dispatcher;
    private readonly SessionTerminator _terminator;
    public PacketSession Session { get; }
    public string RemoteAddress { get; }
    public SessionState State { get; } = new();

    public ClientConnection(
        PacketSession session,
        string remoteAddress,
        PacketDispatcher dispatcher,
        SessionTerminator terminator,
        ILogger<ClientConnection> logger)
    {
        Session = session;
        RemoteAddress = remoteAddress;
        _dispatcher = dispatcher;
        _terminator = terminator;
        _logger = logger;
    }

    public Task RunAsync(CancellationToken ct)
    {
        return Session.RunAsync(OnPacketAsync, ct);
    }

    private async ValueTask OnPacketAsync(MessageId id, ReadOnlyMemory<byte> body, CancellationToken ct)
    {
        try
        {
            await _dispatcher.DispatchAsync(this, id, body, ct).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex,
                "Handler exception for {Id} ({IdHex}) from {Ip}",
                id, $"0x{(ushort)id:X4}", RemoteAddress);
            throw;
        }
    }

    public async ValueTask DisposeAsync()
    {
        // Best-effort cleanup of TCURRENTUSER / TLog on disconnect. Skipped when the session
        // is in MapSvr handoff (CS_START_ACK fired) — MapSvr will own the cleanup once gameplay
        // ends, and wiping the row here would invalidate the dwKEY the client is about to present.
        if (!State.HandoffToMap && State.UserId is int userId && State.Key is uint key)
        {
            await _terminator.TerminateAsync(userId, key, CancellationToken.None).ConfigureAwait(false);
        }
        else if (State.HandoffToMap)
        {
            _logger.LogInformation(
                "Disconnect during MapSvr handoff for user={UserId} key={Key} — leaving TCURRENTUSER intact",
                State.UserId, State.Key);
        }
        await Session.DisposeAsync().ConfigureAwait(false);
    }
}

/// <summary>
/// Routes a decoded packet to the registered handler for its <see cref="MessageId"/>.
/// Handlers are registered at host startup. Each dispatch is wrapped in an
/// <see cref="Activity"/> on the <c>FourStory.Login</c> ActivitySource so trace
/// backends can see one span per handler invocation with tags for the packet ID,
/// the authenticated user, and the result.
/// </summary>
public sealed class PacketDispatcher
{
    internal static readonly ActivitySource ActivitySource = new("FourStory.Login");

    private readonly Dictionary<MessageId, Func<ClientConnection, ReadOnlyMemory<byte>, CancellationToken, ValueTask>> _handlers = new();
    private readonly ILogger<PacketDispatcher> _logger;

    public PacketDispatcher(ILogger<PacketDispatcher> logger)
    {
        _logger = logger;
    }

    public void Register(
        MessageId id,
        Func<ClientConnection, ReadOnlyMemory<byte>, CancellationToken, ValueTask> handler)
    {
        _handlers[id] = handler;
    }

    public async ValueTask DispatchAsync(
        ClientConnection conn,
        MessageId id,
        ReadOnlyMemory<byte> body,
        CancellationToken ct)
    {
        if (!_handlers.TryGetValue(id, out var h))
        {
            _logger.LogWarning(
                "Unhandled packet {Id} (0x{IdHex:X4}) from {Ip}, payload {Bytes} bytes",
                id, (ushort)id, conn.RemoteAddress, body.Length);
            return;
        }

        using var activity = ActivitySource.StartActivity($"handle {id}", ActivityKind.Server);
        if (activity is not null)
        {
            activity.SetTag("packet.id", id.ToString());
            activity.SetTag("packet.id_hex", $"0x{(ushort)id:X4}");
            activity.SetTag("packet.size", body.Length);
            activity.SetTag("net.peer.ip", conn.RemoteAddress);
            if (conn.State.UserId is int uid)
            {
                activity.SetTag("enduser.id", uid);
            }
        }

        try
        {
            await h(conn, body, ct).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            activity?.SetStatus(ActivityStatusCode.Error, ex.Message);
            activity?.AddException(ex);
            throw;
        }
    }
}
