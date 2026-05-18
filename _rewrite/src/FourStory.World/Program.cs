using System.Globalization;
using FourStory.Persistence;
using FourStory.Protocol;
using FourStory.Shared.Observability;
using FourStory.World;
using FourStory.World.Handlers;
using FourStory.World.Network;
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
var worldPort = builder.Configuration.GetValue("World:Port", (int)ProtocolConstants.DefaultWorldPort);
// Which world (TGROUP.bGroupID) this World process owns. C++:
// Server/TWorldSvr/TWorldSvr.cpp::LoadConfig — registry `GroupID`.
var worldGroupId = (byte)builder.Configuration.GetValue("World:GroupId", 1);
// Per-process server id (registry `ServerID`). Map peers send their *own*
// ServerID inside MW_CONNECT_ACK; this is the World process's own identity.
var worldServerId = (byte)builder.Configuration.GetValue("World:ServerId", 1);
var globalConn = builder.Configuration.GetConnectionString("Global")
    ?? "Server=localhost;Database=TGLOBAL_RAGEZONE;Integrated Security=True;TrustServerCertificate=True;";
var gameConn = builder.Configuration.GetConnectionString("Game")
    ?? "Server=localhost;Database=TGAME_RAGEZONE;Integrated Security=True;TrustServerCertificate=True;";

// --- Telemetry ------------------------------------------------------------
builder.Services.AddFourStoryTelemetry(builder.Configuration, "FourStory.World");

// --- EF Core --------------------------------------------------------------
builder.Services.AddDbContextFactory<GlobalDbContext>(opt => opt.UseSqlServer(globalConn));
builder.Services.AddDbContextFactory<GameDbContext>(opt => opt.UseSqlServer(gameConn));

// --- Services -------------------------------------------------------------
builder.Services.AddSingleton(new WorldServerInfo(
    GroupId: worldGroupId,
    ServerId: worldServerId,
    Port: (ushort)worldPort));

builder.Services.AddSingleton<WorldPacketDispatcher>(sp =>
{
    var dispatcher = new WorldPacketDispatcher(sp.GetRequiredService<ILogger<WorldPacketDispatcher>>());
    var mapConnect = ActivatorUtilities.CreateInstance<MapConnectHandler>(sp);
    mapConnect.Register(dispatcher);
    var mapAddChar = ActivatorUtilities.CreateInstance<MapAddCharHandler>(sp);
    mapAddChar.Register(dispatcher);
    return dispatcher;
});

builder.Services.AddHostedService(sp => new WorldServer(
    sp, sp.GetRequiredService<ILogger<WorldServer>>(), worldPort));

var app = builder.Build();
await app.RunAsync().ConfigureAwait(false);
