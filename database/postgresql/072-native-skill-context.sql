-- Written only. Source FINISHSKILL consults the complete guild-skill registry,
-- independently of the skill's kind or whether the actor currently has a guild.
-- Missing publication cannot prove non-membership. No chart row is seeded.
CREATE SCHEMA skill_context_compat;
CREATE TABLE runtime_control.skill_context_catalog (
    singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    character_manifest text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    routing_manifest text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    metadata_sha256 text NOT NULL CHECK(metadata_sha256 ~ '^[0-9a-f]{64}$'),
    guild_rows bigint NOT NULL CHECK(guild_rows>=0),
    guild_sha256 text NOT NULL CHECK(guild_sha256 ~ '^[0-9a-f]{64}$'),
    guild_bytes_sha256 text CHECK(guild_bytes_sha256 ~ '^[0-9a-f]{64}$'),
    activated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE runtime_control.skill_context_activations (
    activation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    previous_run_id bigint REFERENCES reconstruction.import_runs(id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    character_manifest text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    routing_manifest text NOT NULL REFERENCES reconstruction.import_runs(manifest_sha256),
    metadata_sha256 text NOT NULL CHECK(metadata_sha256 ~ '^[0-9a-f]{64}$'),
    guild_rows bigint NOT NULL CHECK(guild_rows>=0),
    guild_sha256 text NOT NULL CHECK(guild_sha256 ~ '^[0-9a-f]{64}$'),
    guild_bytes_sha256 text CHECK(guild_bytes_sha256 ~ '^[0-9a-f]{64}$'),
    activated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE VIEW skill_context_compat.catalog_release AS
    SELECT c.run_id,c.manifest_sha256,c.character_manifest,c.routing_manifest,
        c.metadata_sha256,c.guild_rows,r.status
    FROM runtime_control.skill_context_catalog c
    JOIN reconstruction.import_runs r ON r.id=c.run_id AND r.manifest_sha256=c.manifest_sha256
        AND r.status='verified' AND (r.source_manifest->>'metadata_sha256')=c.metadata_sha256
    JOIN character_compat.catalog_release actor ON actor.manifest_sha256=c.character_manifest AND actor.status='verified'
    JOIN reconstruction.import_runs characters ON characters.id=actor.run_id AND characters.status='verified'
        AND (characters.source_manifest->>'metadata_sha256')=c.metadata_sha256
        AND (r.source_manifest->'backups') @> (characters.source_manifest->'backups')
        AND (characters.source_manifest->'backups') @> (r.source_manifest->'backups')
    JOIN route_compat.catalog_release route ON route.manifest_sha256=c.routing_manifest AND route.status='verified'
    JOIN reconstruction.import_runs routing ON routing.id=route.run_id AND routing.status='verified'
        AND (routing.source_manifest->>'metadata_sha256')=c.metadata_sha256
        AND (r.source_manifest->'backups') @> (routing.source_manifest->'backups')
        AND (routing.source_manifest->'backups') @> (r.source_manifest->'backups')
    JOIN reconstruction.checkpoints g ON g.run_id=c.run_id AND g.source_database='TGAME_RAGEZONE'
        AND g.source_schema='dbo' AND g.source_table='TGUILDSKILLCHART' AND g.row_count=c.guild_rows
        AND g.source_sha256=c.guild_sha256 AND g.target_sha256=c.guild_sha256
        AND g.source_bytes_sha256 IS NOT DISTINCT FROM c.guild_bytes_sha256
        AND g.target_bytes_sha256 IS NOT DISTINCT FROM c.guild_bytes_sha256
    WHERE c.singleton
        AND (SELECT count(*) FROM legacy_game."TGUILDSKILLCHART" s WHERE s."_import_run_id"=c.run_id)=c.guild_rows
        AND NOT EXISTS(SELECT 1 FROM legacy_game."TGUILDSKILLCHART" s
            WHERE s."_import_run_id"=c.run_id AND (s."wSkillID" IS NULL OR s."bType" IS NULL));
CREATE VIEW skill_context_compat."TGUILDSKILLCHART" AS
    SELECT s.* FROM legacy_game."TGUILDSKILLCHART" s
    JOIN skill_context_compat.catalog_release c ON c.run_id=s."_import_run_id";
COMMENT ON VIEW skill_context_compat.catalog_release IS
 'Complete pinned source guild-skill registry, explicitly published offline and bound to current character/routing metadata/backups. Runtime checks checkpoints, actual count and non-NULL keys; actual value/byte hashes are verified by the offline publisher. Missing/stale publication never proves no guild-skill membership. Does not implement guild skills or authority.';
REVOKE ALL ON SCHEMA skill_context_compat FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA skill_context_compat FROM PUBLIC;
REVOKE ALL ON runtime_control.skill_context_catalog,runtime_control.skill_context_activations FROM PUBLIC;
