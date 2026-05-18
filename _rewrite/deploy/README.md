# FourStory dev stack

## Quick start

```pwsh
# 1) Set SA password for the host MSSQL (containers connect via host.docker.internal)
$env:SA_PASSWORD = "YourStrongPassword!"

# 2) Bring the cluster up (login + world + map + Seq + OTel collector)
docker compose --profile all up --build
```

Then point a client at `127.0.0.1:4815` (login) and watch traces at <http://localhost:5341>.

## Profiles

The compose file uses Compose profiles so you can run subsets of the stack:

| Command | What runs |
|---------|-----------|
| `docker compose up` | Nothing (no profile = everything is opt-in). |
| `docker compose --profile observability up` | Just Seq + OTel collector — useful when you run the .NET workers from your IDE. |
| `docker compose --profile all up` | Full stack including the three game workers. |

## Environment expectations

| Variable | Purpose |
|----------|---------|
| `SA_PASSWORD` | Required when running with `--profile all`. The workers connect to your host's MSSQL instance via `host.docker.internal` and SQL auth. If you prefer Windows auth on the host, switch the connection strings to use a domain account and remove the `User Id=sa;Password=…` bits. |

## Telemetry on / off

In each worker `Program.cs`, `AddFourStoryTelemetry(...)` only registers OTel if either:
- `Telemetry:Enabled = true` in appsettings, **or**
- `OTEL_EXPORTER_OTLP_ENDPOINT` env var is set.

Both are pre-set in the compose file. Running locally without OTel? Unset them — the workers run with zero telemetry overhead.

## Health checks

(Wired up in workers via `MapHealthChecks("/healthz")`. The login worker is currently a pure Worker host so it doesn't have an HTTP endpoint yet — health checks come online once we move to a `WebApplication` host or add a minimal Kestrel listener.)

## What's NOT in this compose yet

- **PostgreSQL** — we're still on MSSQL until the persistence migration lands.
- **DB seeding** — the legacy `.bak` files are restored on the host, not in the container.
- **Patch / Log / BR / BoW servers** — not yet ported.
- **HTTPS / cert management** — Login/Map/World listen on raw TCP only.
