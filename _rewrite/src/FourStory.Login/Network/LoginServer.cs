using System.Net;
using System.Net.Sockets;
using FourStory.Protocol.Session;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Handlers;

/// <summary>
/// Hosted service: listens on the configured login port and spawns a
/// <see cref="ClientConnection"/> per accepted TCP socket.
/// </summary>
public sealed class LoginServer : BackgroundService
{
    private readonly IServiceProvider _services;
    private readonly ILogger<LoginServer> _logger;
    private readonly int _port;

    public LoginServer(IServiceProvider services, ILogger<LoginServer> logger, int port)
    {
        _services = services;
        _logger = logger;
        _port = port;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        var listener = new TcpListener(IPAddress.Any, _port);
        listener.Start();
        _logger.LogInformation("Login server listening on 0.0.0.0:{Port}", _port);

        try
        {
            while (!stoppingToken.IsCancellationRequested)
            {
                var socket = await listener.AcceptSocketAsync(stoppingToken).ConfigureAwait(false);
                _ = HandleAcceptedSocketAsync(socket, stoppingToken);
            }
        }
        catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
        {
            // Shutdown
        }
        finally
        {
            listener.Stop();
        }
    }

    private async Task HandleAcceptedSocketAsync(Socket socket, CancellationToken ct)
    {
        var remote = socket.RemoteEndPoint?.ToString() ?? "unknown";
        var ipOnly = remote.Split(':')[0];
        _logger.LogInformation("Client connected: {Remote}", remote);

        using var scope = _services.CreateScope();
        var dispatcher = scope.ServiceProvider.GetRequiredService<PacketDispatcher>();
        var terminator = scope.ServiceProvider.GetRequiredService<FourStory.Login.Auth.SessionTerminator>();
        var registry = scope.ServiceProvider.GetService<FourStory.Login.Services.ConnectionRegistry>();
        var logger = scope.ServiceProvider.GetRequiredService<ILogger<ClientConnection>>();

        await using var stream = new NetworkStream(socket, ownsSocket: true);
        var codec = new PacketCodec(PeerType.Client);
        await using var session = new PacketSession(stream, codec, ownsTransport: false);
        var conn = new ClientConnection(session, ipOnly, dispatcher, terminator, logger);

        registry?.Add(conn);
        try
        {
            await conn.RunAsync(ct).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Client {Remote} disconnected with error", remote);
        }
        finally
        {
            registry?.Remove(conn);
            _logger.LogInformation("Client disconnected: {Remote}", remote);
        }
    }
}
