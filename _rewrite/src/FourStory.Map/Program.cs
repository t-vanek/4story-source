using System.Globalization;
using FourStory.Map;
using FourStory.Map.Handlers;
using FourStory.Map.Network;
using FourStory.Map.Services;
using FourStory.Persistence;
using FourStory.Protocol;
using FourStory.Shared.Observability;
using Microsoft.EntityFrameworkCore;
using Orleans.Hosting;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Serilog;

var builder = Host.CreateApplicationBuilder(args);

// --- Logging ---------------------------------------------------------------
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
var mapPort = builder.Configuration.GetValue("Map:Port", (int)ProtocolConstants.DefaultMapPort);
// Which world (TGROUP.bGroupID) this Map process is serving. Stamped into
// TCURRENTUSER on CS_CONNECT so TLoginSvr / TWorldSvr can route hand-offs.
var mapGroupId = (byte)builder.Configuration.GetValue("Map:GroupId", 1);
// Per-process Map server id. Sent to World in MW_CONNECT_ACK so the cluster
// can route per-character SS traffic back to this process.
// C++: registry `ServerID` (Server/TMapSvr/TMapSvr.cpp::LoadConfig).
var mapServerId = (byte)builder.Configuration.GetValue("Map:ServerId", 1);
// Channels this Map process serves. Advertised to World in the same
// MW_CONNECT_ACK payload. C++: populated from TCHANNEL rows in m_mapTLOGCHANNEL.
var mapChannels = builder.Configuration.GetSection("Map:Channels").Get<byte[]>()
    ?? new byte[] { 1 };

// Outbound WorldSvr endpoint. C++: registry `WorldIP` + `WorldPort` loaded in
// Server/TMapSvr/TMapSvr.cpp:683/737 with fallback to DEF_WORLDPORT.
var worldHost = builder.Configuration.GetValue<string>("World:Host") ?? "127.0.0.1";
var worldPort = (ushort)builder.Configuration.GetValue("World:Port", (int)ProtocolConstants.DefaultWorldPort);

var globalConn = builder.Configuration.GetConnectionString("Global")
    ?? "Server=localhost;Database=TGLOBAL_RAGEZONE;Integrated Security=True;TrustServerCertificate=True;";
var gameConn = builder.Configuration.GetConnectionString("Game")
    ?? "Server=localhost;Database=TGAME_RAGEZONE;Integrated Security=True;TrustServerCertificate=True;";

// --- Orleans silo ---------------------------------------------------------
// Localhost clustering for dev — single silo, in-memory grain state. Replace
// with redis/sql cluster + Postgres state provider in production.
builder.UseOrleans(silo =>
{
    silo.UseLocalhostClustering();
    silo.AddMemoryGrainStorageAsDefault();
});

// --- Telemetry ------------------------------------------------------------
builder.Services.AddFourStoryTelemetry(builder.Configuration, "FourStory.Map");

// --- EF Core --------------------------------------------------------------
builder.Services.AddDbContextFactory<GlobalDbContext>(opt => opt.UseSqlServer(globalConn));
builder.Services.AddDbContextFactory<GameDbContext>(opt => opt.UseSqlServer(gameConn));

// --- Services -------------------------------------------------------------
builder.Services.AddSingleton(new MapServerInfo(
    GroupId: mapGroupId,
    ServerId: mapServerId,
    Port: (ushort)mapPort));
builder.Services.AddSingleton(new WorldEndpoint(Host: worldHost, Port: worldPort));
builder.Services.AddSingleton(new MapChannelList(mapChannels));
builder.Services.AddSingleton<MapConnectionRegistry>();
builder.Services.AddSingleton<MapInitOrchestrator>();
builder.Services.AddSingleton<WorldClientDispatcher>(sp =>
{
    var dispatcher = new WorldClientDispatcher(sp.GetRequiredService<ILogger<WorldClientDispatcher>>());
    var enterSvr = ActivatorUtilities.CreateInstance<EnterSvrHandler>(sp);
    enterSvr.Register(dispatcher);
    return dispatcher;
});

// WorldClient is BOTH a singleton (so handlers can inject it) AND the hosted
// service that runs its accept loop. Registering it as a singleton first and
// then handing the same instance to the hosted-service collection keeps the
// graph well-defined (otherwise ASP.NET would build two copies).
builder.Services.AddSingleton<WorldClient>();
builder.Services.AddHostedService(sp => sp.GetRequiredService<WorldClient>());

builder.Services.AddSingleton<MapPacketDispatcher>(sp =>
{
    var dispatcher = new MapPacketDispatcher(sp.GetRequiredService<ILogger<MapPacketDispatcher>>());
    var connect = ActivatorUtilities.CreateInstance<ConnectHandler>(sp);
    connect.Register(dispatcher);
    var ready = ActivatorUtilities.CreateInstance<ReadyHandler>(sp);
    ready.Register(dispatcher);
    return dispatcher;
});

builder.Services.AddHostedService(sp => new MapServer(
    sp, sp.GetRequiredService<ILogger<MapServer>>(), mapPort));

var app = builder.Build();
await app.RunAsync().ConfigureAwait(false);
