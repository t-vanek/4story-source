using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using OpenTelemetry.Metrics;
using OpenTelemetry.Resources;
using OpenTelemetry.Trace;

namespace FourStory.Shared.Observability;

/// <summary>
/// Adds OpenTelemetry tracing + metrics to a host. Exporter is OTLP by default,
/// can be disabled by leaving <c>OTEL_EXPORTER_OTLP_ENDPOINT</c> unset (or via
/// <c>Telemetry:Enabled = false</c> in appsettings) — in which case OTel is registered
/// but spans / metrics go nowhere, with negligible overhead.
///
/// One call site per worker keeps the per-process boilerplate small and the
/// resource attributes (service.name + service.version) consistent.
/// </summary>
public static class TelemetrySetup
{
    public static IServiceCollection AddFourStoryTelemetry(
        this IServiceCollection services,
        IConfiguration configuration,
        string serviceName)
    {
        var enabled = configuration.GetValue("Telemetry:Enabled", false);
        var otlpEndpoint = configuration.GetValue<string?>("OTEL_EXPORTER_OTLP_ENDPOINT")
            ?? Environment.GetEnvironmentVariable("OTEL_EXPORTER_OTLP_ENDPOINT");

        // If neither config explicitly enables nor an endpoint is set, skip registration —
        // there's no point spinning up an exporter that has nowhere to send.
        if (!enabled && string.IsNullOrEmpty(otlpEndpoint))
        {
            return services;
        }

        var resource = ResourceBuilder.CreateDefault()
            .AddService(serviceName: serviceName, serviceVersion: GetVersion())
            .AddAttributes(new Dictionary<string, object>
            {
                ["deployment.environment"] = configuration.GetValue("ASPNETCORE_ENVIRONMENT", "production"),
            });

        services.AddOpenTelemetry()
            .ConfigureResource(_ => _.AddService(serviceName, serviceVersion: GetVersion()))
            .WithTracing(t => t
                .SetResourceBuilder(resource)
                .AddSource("FourStory.*")
                .AddOtlpExporter())
            .WithMetrics(m => m
                .SetResourceBuilder(resource)
                .AddMeter("FourStory.*")
                .AddRuntimeInstrumentation()
                .AddOtlpExporter());

        return services;
    }

    private static string GetVersion()
    {
        var asm = System.Reflection.Assembly.GetEntryAssembly();
        return asm?.GetName().Version?.ToString() ?? "0.0.0";
    }
}
