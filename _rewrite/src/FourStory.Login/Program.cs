using System.Globalization;
using FourStory.Login.Auth;
using FourStory.Login.Handlers;
using FourStory.Login.Services;
using FourStory.Persistence;
using FourStory.Protocol;
using FourStory.Shared.Observability;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Serilog;

var builder = Host.CreateApplicationBuilder(args);

// --- Logging --------------------------------------------------------------
Log.Logger = new LoggerConfiguration()
    .WriteTo.Console(
        outputTemplate: "[{Timestamp:HH:mm:ss.fff} {Level:u3}] {SourceContext} {Message:lj}{NewLine}{Exception}",
        formatProvider: CultureInfo.InvariantCulture)
    .MinimumLevel.Information()
    .Enrich.FromLogContext()
    .CreateLogger();

builder.Logging.ClearProviders();
builder.Logging.AddSerilog(dispose: true);

// --- Configuration --------------------------------------------------------
var loginPort = builder.Configuration.GetValue("Login:Port", 4815);
var healthPort = builder.Configuration.GetValue("Login:HealthPort", 8081);
var globalConn = builder.Configuration.GetConnectionString("Global")
    ?? "Server=localhost;Database=TGLOBAL_RAGEZONE;Integrated Security=True;TrustServerCertificate=True;";
var gameConn = builder.Configuration.GetConnectionString("Game")
    ?? "Server=localhost;Database=TGAME_RAGEZONE;Integrated Security=True;TrustServerCertificate=True;";

// --- Telemetry ------------------------------------------------------------
builder.Services.AddFourStoryTelemetry(builder.Configuration, "FourStory.Login");

// --- EF Core --------------------------------------------------------------
builder.Services.AddDbContextFactory<GlobalDbContext>(opt =>
    opt.UseSqlServer(globalConn));
builder.Services.AddDbContextFactory<GameDbContext>(opt =>
    opt.UseSqlServer(gameConn));

// --- Services -------------------------------------------------------------
builder.Services.AddSingleton<MapServerLocator>();
builder.Services.AddSingleton<PasswordHasher>();
builder.Services.AddSingleton<LoginRateLimiter>();
builder.Services.AddSingleton<ConnectionRegistry>();
builder.Services.AddHostedService<IdleSessionMonitor>();
builder.Services.AddHostedService<StaleSessionCleaner>();
builder.Services.AddSingleton<IAuthService, AuthService>();
builder.Services.AddSingleton<CharService>();
builder.Services.AddSingleton<SessionTerminator>();
builder.Services.AddSingleton<PacketDispatcher>(sp =>
{
    var dispatcher = new PacketDispatcher(sp.GetRequiredService<ILogger<PacketDispatcher>>());
    var loginHandler = ActivatorUtilities.CreateInstance<LoginHandler>(sp);
    dispatcher.Register(MessageId.CS_LOGIN_REQ, loginHandler.HandleAsync);
    var lobbyHandlers = ActivatorUtilities.CreateInstance<LobbyHandlers>(sp);
    lobbyHandlers.Register(dispatcher);
    return dispatcher;
});

builder.Services.AddHostedService(sp => new LoginServer(
    sp,
    sp.GetRequiredService<ILogger<LoginServer>>(),
    loginPort));

builder.Services.AddHostedService(sp => new HealthEndpointService(
    sp.GetRequiredService<IDbContextFactory<GlobalDbContext>>(),
    sp.GetRequiredService<ILogger<HealthEndpointService>>(),
    healthPort));

var app = builder.Build();
await app.RunAsync().ConfigureAwait(false);
