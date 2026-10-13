-- Explicit offline publication of the full original TGUILDCHART. Neither an
-- empty migration nor the latest archival run implicitly activates gameplay.
CREATE SCHEMA world_compat;
CREATE TABLE runtime_control.world_guild_level_catalog (
    singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    value_sha256 text NOT NULL CHECK(value_sha256 ~ '^[0-9a-f]{64}$'),
    row_count bigint NOT NULL CHECK(row_count=11),
    activated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE runtime_control.world_guild_level_activations (
    activation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    previous_run_id bigint REFERENCES reconstruction.import_runs(id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    value_sha256 text NOT NULL CHECK(value_sha256 ~ '^[0-9a-f]{64}$'),
    activated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE VIEW world_compat.guild_level_release AS
    SELECT c.run_id,c.manifest_sha256,c.value_sha256,c.row_count,r.status
    FROM runtime_control.world_guild_level_catalog c
    JOIN reconstruction.import_runs r ON r.id=c.run_id AND r.manifest_sha256=c.manifest_sha256
    JOIN reconstruction.checkpoints p ON p.run_id=c.run_id AND p.source_database='TGAME_RAGEZONE'
        AND p.source_schema='dbo' AND p.source_table='TGUILDCHART' AND p.row_count=c.row_count
        AND p.source_sha256=c.value_sha256 AND p.target_sha256=c.value_sha256
    WHERE c.singleton AND r.status='verified';
CREATE VIEW world_compat."TGUILDCHART" AS
    SELECT s.* FROM legacy_game."TGUILDCHART" s
    JOIN world_compat.guild_level_release c ON c.run_id=s."_import_run_id";
COMMENT ON VIEW world_compat."TGUILDCHART" IS
 'Original15-column guild level chart, including source level0. Full pinned archival values must be verified and explicitly published offline; no fabricated default caps.';
REVOKE ALL ON SCHEMA world_compat FROM PUBLIC;
REVOKE ALL ON world_compat.guild_level_release,world_compat."TGUILDCHART",
    runtime_control.world_guild_level_catalog,runtime_control.world_guild_level_activations FROM PUBLIC;
