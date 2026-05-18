using System.Net;
using System.Net.Sockets;
using FourStory.Protocol.Session;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace FourStory.Map.Network;

public sealed class MapServer : BackgroundService
{
    private readonly IServiceProvider _services;
    private readonly ILogger<MapServer> _logger;
    private readonly int _port;

    public MapServer(IServiceProvider services, ILogger<MapServer> logger, int port)
    {
        _services = services;
        _logger = logger;
        _port = port;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        var listener = new TcpListener(IPAddress.Any, _port);
        listener.Start();
        _logger.LogInformation("Map server listening on 0.0.0.0:{Port}", _port);
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
        // Capture the local endpoint (this server's IP and port as the client
        // connected to it) so handlers can stamp it into TCURRENTUSER — mirrors
        // Server/TMapSvr/CSHandler.cpp::OnCS_CONNECT_REQ which records
        // m_pTextInfo->m_szIPAddr / m_wPort.
        var localEp = socket.LocalEndPoint as IPEndPoint;
        var localIp = localEp?.Address.ToString() ?? "0.0.0.0";
        var localPort = (ushort)(localEp?.Port ?? _port);
        _logger.LogInformation("Map client connected: {Remote} -> {Local}", remote, localEp);

        using var scope = _services.CreateScope();
        var dispatcher = scope.ServiceProvider.GetRequiredService<MapPacketDispatcher>();
        var logger = scope.ServiceProvider.GetRequiredService<ILogger<MapConnection>>();
        // Registry is a process-level singleton — fetched from the root provider via
        // the scope, but its lifetime is independent of the scope (singletons resolve
        // to the same instance regardless of scope).
        var registry = scope.ServiceProvider.GetRequiredService<MapConnectionRegistry>();

        await using var stream = new NetworkStream(socket, ownsSocket: true);
        var codec = new PacketCodec(PeerType.Client);
        await using var session = new PacketSession(stream, codec, ownsTransport: false);
        var conn = new MapConnection(session, ipOnly, localIp, localPort, dispatcher, logger, registry);

        try
        {
            await conn.RunAsync(ct).ConfigureAwait(false);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Map client {Remote} disconnected with error", remote);
        }
        finally
        {
            _logger.LogInformation("Map client disconnected: {Remote}", remote);
        }
    }
}
