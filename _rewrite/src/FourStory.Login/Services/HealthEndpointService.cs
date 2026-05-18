using System.Net;
using System.Text;
using FourStory.Persistence;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace FourStory.Login.Services;

/// <summary>
/// Tiny HTTP listener exposing <c>/healthz</c> (liveness) and <c>/readyz</c>
/// (readiness — pings the Global DB). Deliberately built on
/// <see cref="HttpListener"/> instead of full ASP.NET Core to keep the Worker
/// SDK lean — health endpoints are the only HTTP surface this server needs.
///
/// Metrics are exported via OpenTelemetry OTLP separately (see Telemetry.cs);
/// we don't expose a Prometheus scrape endpoint here.
/// </summary>
public sealed class HealthEndpointService : BackgroundService
{
    private readonly HttpListener _listener = new();
    private readonly IDbContextFactory<GlobalDbContext> _dbFactory;
    private readonly ILogger<HealthEndpointService> _logger;
    private readonly int _port;

    public HealthEndpointService(
        IDbContextFactory<GlobalDbContext> dbFactory,
        ILogger<HealthEndpointService> logger,
        int port)
    {
        _dbFactory = dbFactory;
        _logger = logger;
        _port = port;
        _listener.Prefixes.Add($"http://+:{port}/healthz/");
        _listener.Prefixes.Add($"http://+:{port}/readyz/");
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        try
        {
            _listener.Start();
        }
        catch (HttpListenerException ex)
        {
            // On Windows, binding to http://+:port/ requires netsh urlacl. Fall back to localhost.
            _logger.LogWarning(ex, "Failed to bind health listener on http://+:{Port} — falling back to 127.0.0.1", _port);
            _listener.Prefixes.Clear();
            _listener.Prefixes.Add($"http://127.0.0.1:{_port}/healthz/");
            _listener.Prefixes.Add($"http://127.0.0.1:{_port}/readyz/");
            _listener.Start();
        }
        _logger.LogInformation("Health endpoint listening on port {Port}", _port);

        stoppingToken.Register(() =>
        {
            try { _listener.Stop(); } catch { /* shutting down */ }
        });

        while (!stoppingToken.IsCancellationRequested)
        {
            HttpListenerContext ctx;
            try
            {
                ctx = await _listener.GetContextAsync().ConfigureAwait(false);
            }
            catch (HttpListenerException) when (stoppingToken.IsCancellationRequested)
            {
                return;
            }
            catch (ObjectDisposedException)
            {
                return;
            }

            _ = Task.Run(() => HandleAsync(ctx, stoppingToken), stoppingToken);
        }
    }

    private async Task HandleAsync(HttpListenerContext ctx, CancellationToken ct)
    {
        try
        {
            var path = ctx.Request.Url?.AbsolutePath ?? "/";
            switch (path.TrimEnd('/'))
            {
                case "/healthz":
                    await WriteAsync(ctx, 200, "ok").ConfigureAwait(false);
                    break;
                case "/readyz":
                    var ok = await CheckReadyAsync(ct).ConfigureAwait(false);
                    await WriteAsync(ctx, ok ? 200 : 503, ok ? "ready" : "db-unavailable").ConfigureAwait(false);
                    break;
                default:
                    await WriteAsync(ctx, 404, "not found").ConfigureAwait(false);
                    break;
            }
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Health endpoint handler failed");
            try { ctx.Response.StatusCode = 500; ctx.Response.Close(); } catch { /* best-effort */ }
        }
    }

    private async Task<bool> CheckReadyAsync(CancellationToken ct)
    {
        try
        {
            await using var db = await _dbFactory.CreateDbContextAsync(ct).ConfigureAwait(false);
            return await db.Database.CanConnectAsync(ct).ConfigureAwait(false);
        }
        catch
        {
            return false;
        }
    }

    private static async Task WriteAsync(HttpListenerContext ctx, int status, string body)
    {
        ctx.Response.StatusCode = status;
        ctx.Response.ContentType = "text/plain";
        var bytes = Encoding.UTF8.GetBytes(body);
        ctx.Response.ContentLength64 = bytes.Length;
        await ctx.Response.OutputStream.WriteAsync(bytes).ConfigureAwait(false);
        ctx.Response.OutputStream.Close();
    }
}
