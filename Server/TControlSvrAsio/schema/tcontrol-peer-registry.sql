-- TPEER_REGISTRY — durable snapshot of the modern peer registry
-- (TControlSvrAsio's CT_PEER_REGISTER_REQ table). Apply once per
-- TGLOBAL database before enabling [registry.persistence] in the
-- TControlSvrAsio config.
--
-- The table is independent of the legacy TSERVER inventory. TSERVER
-- holds the static topology (which services are provisioned + their
-- ports + their target machines). TPEER_REGISTRY holds the dynamic
-- "which provisioned services are currently running + reachable"
-- view, refreshed by peer-side heartbeats every 30s. Restart of
-- TControl reloads TPEER_REGISTRY so the cluster picture is
-- immediately accurate without waiting for the first round of
-- heartbeats to come in.
--
-- Schema choices:
--   * Timestamps stored as BIGINT unix seconds — portable across
--     MSSQL/PostgreSQL/SQLite, no DATETIME dialect headaches, and
--     easier to compare against std::chrono::system_clock::now()
--     in the C++ side.
--   * dwServiceID is the PK — re-registration of the same service
--     replaces the row in a DELETE+INSERT transaction (see
--     SociRegistryPersistence::Upsert).
--   * No FK to TSERVER — peers may register a service_id that the
--     static inventory hasn't seen yet (in-flight DB migration,
--     auto-provisioning); the CT_PEER_REGISTER_REQ handler does the
--     inventory check before calling Upsert.
--
-- The C++ side validates this DDL exists at boot when
-- [registry.persistence] enabled = true. Missing → boot fails fast
-- with the path to this file in the error message.

CREATE TABLE "TPEER_REGISTRY" (
    "dwServiceID"          INT          NOT NULL PRIMARY KEY,
    "szReportedName"       VARCHAR(64)  NOT NULL,
    "szReportedAddr"       VARCHAR(45)  NOT NULL,
    "wReportedPort"        INT          NOT NULL,
    "szVersion"            VARCHAR(64)  NOT NULL,
    "dwPid"                INT          NOT NULL,
    "qwStartUnix"          BIGINT       NOT NULL,
    "dwCurUsers"           INT          NOT NULL DEFAULT 0,
    "dwMaxUsers"           INT          NOT NULL DEFAULT 0,
    "qwLeaseEpoch"         BIGINT       NOT NULL,
    "tRegisteredUnix"      BIGINT       NOT NULL,
    "tLastHeartbeatUnix"   BIGINT       NOT NULL
);

-- Index for the lease-expiry sweep, which scans by tLastHeartbeatUnix
-- on every tick. Negligible on small registries (<1k peers), free
-- insurance for larger deploys.
CREATE INDEX "IX_TPEER_REGISTRY_LastHeartbeat"
    ON "TPEER_REGISTRY"("tLastHeartbeatUnix");
