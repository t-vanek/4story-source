-- Written only. Ordinary lethal self damage requires explicit complete source
-- map-context evidence; no missing publication is interpreted as an empty chart.
-- TBATTLEZONECHART bounds possible castle/local participation, TGODBALLCHART
-- bounds the source GodBall map graph. No player state or chart value is seeded.
CREATE SCHEMA death_compat;
CREATE TABLE runtime_control.death_context_catalog (
    singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    routing_manifest text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    metadata_sha256 text NOT NULL CHECK(metadata_sha256 ~ '^[0-9a-f]{64}$'),
    zone_rows bigint NOT NULL CHECK(zone_rows>=0),
    zone_sha256 text NOT NULL CHECK(zone_sha256 ~ '^[0-9a-f]{64}$'),
    zone_bytes_sha256 text NOT NULL CHECK(zone_bytes_sha256 ~ '^[0-9a-f]{64}$'),
    ball_rows bigint NOT NULL CHECK(ball_rows>=0),
    ball_sha256 text NOT NULL CHECK(ball_sha256 ~ '^[0-9a-f]{64}$'),
    ball_bytes_sha256 text CHECK(ball_bytes_sha256 ~ '^[0-9a-f]{64}$'),
    activated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE runtime_control.death_context_activations (
    activation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    previous_run_id bigint REFERENCES reconstruction.import_runs(id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    routing_manifest text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    metadata_sha256 text NOT NULL CHECK(metadata_sha256 ~ '^[0-9a-f]{64}$'),
    zone_rows bigint NOT NULL CHECK(zone_rows>=0),
    zone_sha256 text NOT NULL CHECK(zone_sha256 ~ '^[0-9a-f]{64}$'),
    zone_bytes_sha256 text NOT NULL CHECK(zone_bytes_sha256 ~ '^[0-9a-f]{64}$'),
    ball_rows bigint NOT NULL CHECK(ball_rows>=0),
    ball_sha256 text NOT NULL CHECK(ball_sha256 ~ '^[0-9a-f]{64}$'),
    ball_bytes_sha256 text CHECK(ball_bytes_sha256 ~ '^[0-9a-f]{64}$'),
    activated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE VIEW death_compat.catalog_release AS
    SELECT c.run_id,c.manifest_sha256,c.routing_manifest,c.metadata_sha256,
        c.zone_rows,c.ball_rows,r.status
    FROM runtime_control.death_context_catalog c
    JOIN reconstruction.import_runs r ON r.id=c.run_id AND r.manifest_sha256=c.manifest_sha256
        AND r.status='verified' AND (r.source_manifest->>'metadata_sha256')=c.metadata_sha256
    JOIN route_compat.catalog_release route ON route.manifest_sha256=c.routing_manifest AND route.status='verified'
    JOIN reconstruction.import_runs routing ON routing.id=route.run_id AND routing.status='verified'
        AND (routing.source_manifest->>'metadata_sha256')=c.metadata_sha256
        AND (r.source_manifest->'backups') @> (routing.source_manifest->'backups')
        AND (routing.source_manifest->'backups') @> (r.source_manifest->'backups')
    JOIN reconstruction.checkpoints z ON z.run_id=c.run_id AND z.source_database='TGAME_RAGEZONE'
        AND z.source_schema='dbo' AND z.source_table='TBATTLEZONECHART' AND z.row_count=c.zone_rows
        AND z.source_sha256=c.zone_sha256 AND z.target_sha256=c.zone_sha256
        AND z.source_bytes_sha256=c.zone_bytes_sha256 AND z.target_bytes_sha256=c.zone_bytes_sha256
    JOIN reconstruction.checkpoints b ON b.run_id=c.run_id AND b.source_database='TGAME_RAGEZONE'
        AND b.source_schema='dbo' AND b.source_table='TGODBALLCHART' AND b.row_count=c.ball_rows
        AND b.source_sha256=c.ball_sha256 AND b.target_sha256=c.ball_sha256
        AND b.source_bytes_sha256 IS NOT DISTINCT FROM c.ball_bytes_sha256
        AND b.target_bytes_sha256 IS NOT DISTINCT FROM c.ball_bytes_sha256
    WHERE c.singleton
        AND (SELECT count(*) FROM legacy_game."TBATTLEZONECHART" s WHERE s."_import_run_id"=c.run_id)=c.zone_rows
        AND (SELECT count(*) FROM legacy_game."TGODBALLCHART" s WHERE s."_import_run_id"=c.run_id)=c.ball_rows;
CREATE VIEW death_compat."TBATTLEZONECHART" AS
    SELECT s.* FROM legacy_game."TBATTLEZONECHART" s
    JOIN death_compat.catalog_release c ON c.run_id=s."_import_run_id";
CREATE VIEW death_compat."TGODBALLCHART" AS
    SELECT s.* FROM legacy_game."TGODBALLCHART" s
    JOIN death_compat.catalog_release c ON c.run_id=s."_import_run_id";
COMMENT ON VIEW death_compat.catalog_release IS
 'Explicit offline publication of complete pinned source battle-zone and GodBall charts, matched to the active routing metadata/backups. Missing or stale evidence is unavailable, never an empty-map proof. Does not implement castle/local/GodBall gameplay.';
REVOKE ALL ON SCHEMA death_compat FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA death_compat FROM PUBLIC;
REVOKE ALL ON runtime_control.death_context_catalog,runtime_control.death_context_activations FROM PUBLIC;
