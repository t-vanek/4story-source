using System.Net;
using System.Net.Sockets;
using FourStory.Protocol.Session;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace FourStory.World.Network;

/// <summary>
/// TCP accept loop for the World server. Mirrors
/// <c>Server/TWorldSvr/TWorldSvr.cpp::InitNetwork</c> (line 487) + the
/// <c>WaitForConnect</c>/<c>AcceptEx</c> IOCP pump (line 531). Unlike
/// <c>FourStory.Map.Network.MapServer</c> the accepted peers here are other
/// server processes (Map / Login / DM / Manager), so the codec is
/// instantiated with <see cref="PeerType.Server"/> — no RC4 layer, plaintext
/// inter-server traffic.
/// </summary>
public sealed class WorldServer : BackgroundService
{
    private readonly IServiceProvider _services;
    private readonly ILogger<WorldServer> _logger;
    private readonly int _port;

    public WorldServer(IServiceProvider services, ILogger<WorldServer> logger, int port)
    {
        _services = services;
        _logger = logger;
        _port = port;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        var listener = new TcpListener(IPAddress.Any, _port);
        listener.Start();
        _logger.LogInformation("World server listening on 0.0.0.0:{Port}", _port);
        try
        {
            while (!stoppingToken.IsCancellationRequested)
            {
                var socket = await listener.AcceptSocketAsync(stoppingToken).ConfigureAwait(false);
                _ = HandleAcceptedSocketAsync(socket, stoppingToken);
            }
        }
        catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested) { }
        finally
        {
            listener.Stop();
        }
    }

    private async Task HandleAcceptedSocketAsync(Socket socket, CancellationToken ct)
    {
        var remote = socket.RemoteEndPoint?.ToString() ?? "unknown";
        var ipOnly = remote.Split(':')[0];
        _logger.LogInformation("World peer connected: {Remote}", remote);

        using var scope = _services.CreateScope();
        var dispatcher = scope.ServiceProvider.GetRequiredService<WorldPacketDispatcher>();
        var logger = scope.ServiceProvider.GetRequiredService<ILogger<WorldConnection>>();

        await using var stream = new NetworkStream(socket, ownsSocket: true);
        // PeerType.Server: SS (server-to-server) traffic, no client RC4 layer.
        // Matches C++ TWorldSvr — the listen socket only services other server
        // processes, never end-user clients.
        var codec = new PacketCodec(PeerType.Server);
        await using var session = new PacketSession(stream, codec, ownsTransport: false);
        var conn = new WorldConnection(session, ipOnly, dispatcher, logger);

        try
        {
            await conn.RunAsync(ct).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "World peer {Remote} disconnected with error", remote);
        }
        finally
        {
            _logger.LogInformation("World peer disconnected: {Remote}", remote);
        }
    }
}
