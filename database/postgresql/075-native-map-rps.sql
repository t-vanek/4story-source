-- Phase A only: no migration/import/grants executed. Bind the actual published
-- World chart to the currently selected Map/item/actor/routing/message releases.
CREATE VIEW game_compat.rps_release AS
    SELECT r.world_id,r.run_id,r.manifest_sha256,r.chart_rows,
        m.manifest_sha256 AS map_manifest,c.manifest_sha256 AS character_manifest,
        a.manifest_sha256 AS actor_manifest,t.manifest_sha256 AS routing_manifest
    FROM world_compat.rps_release r
    JOIN reconstruction.import_runs source ON source.id=r.run_id AND source.status='verified'
    CROSS JOIN game_compat.catalog_release m
    CROSS JOIN character_compat.catalog_release c
    CROSS JOIN actor_compat.catalog_release a
    CROSS JOIN route_compat.catalog_release t
    JOIN reconstruction.import_runs mi ON mi.id=m.run_id AND mi.status='verified'
    JOIN reconstruction.import_runs ci ON ci.id=c.run_id AND ci.status='verified'
    JOIN reconstruction.import_runs ai ON ai.id=a.run_id AND ai.status='verified'
    JOIN reconstruction.import_runs ti ON ti.id=t.run_id AND ti.status='verified'
    JOIN game_compat.server_message_release text_release ON text_release.map_manifest=m.manifest_sha256
        AND text_release.character_manifest=c.manifest_sha256 AND text_release.actor_manifest=a.manifest_sha256
        AND text_release.status='verified'
    WHERE m.status='verified' AND c.status='verified' AND a.status='verified' AND t.status='verified'
        AND (source.source_manifest->>'metadata_sha256')=(mi.source_manifest->>'metadata_sha256')
        AND (source.source_manifest->>'metadata_sha256')=(ci.source_manifest->>'metadata_sha256')
        AND (source.source_manifest->>'metadata_sha256')=(ai.source_manifest->>'metadata_sha256')
        AND (source.source_manifest->>'metadata_sha256')=(ti.source_manifest->>'metadata_sha256')
        AND (source.source_manifest->'backups') @> (mi.source_manifest->'backups')
        AND (mi.source_manifest->'backups') @> (source.source_manifest->'backups')
        AND (source.source_manifest->'backups') @> (ci.source_manifest->'backups')
        AND (ci.source_manifest->'backups') @> (source.source_manifest->'backups')
        AND (source.source_manifest->'backups') @> (ai.source_manifest->'backups')
        AND (ai.source_manifest->'backups') @> (source.source_manifest->'backups')
        AND (source.source_manifest->'backups') @> (ti.source_manifest->'backups')
        AND (ti.source_manifest->'backups') @> (source.source_manifest->'backups')
        AND (SELECT count(*) FROM world_compat."TRPSGAMECHART" g WHERE g.world_id=r.world_id)=r.chart_rows;
CREATE VIEW game_compat."TRPSGAMECHART" AS
    SELECT s.* FROM world_compat."TRPSGAMECHART" s
    JOIN game_compat.rps_release r ON r.world_id=s.world_id AND r.run_id=s."_import_run_id";
ALTER TABLE app_world.quest_operations
    ADD COLUMN before_rps jsonb,
    ADD COLUMN after_rps jsonb,
    ADD COLUMN rps_context jsonb,
    ADD CONSTRAINT rps_operation_shape CHECK (
        (before_rps IS NULL AND after_rps IS NULL AND rps_context IS NULL) OR
        (before_rps IS NOT NULL AND after_rps IS NOT NULL AND rps_context IS NOT NULL
            AND jsonb_typeof(before_rps)='array' AND jsonb_array_length(before_rps)=6
            AND jsonb_typeof(after_rps)='array' AND jsonb_array_length(after_rps)=6
            AND jsonb_typeof(rps_context)='object'));
-- Reuse061 TPOSTERRORTABLE and059+ mail ownership/notification structures.
COMMENT ON COLUMN app_world.quest_operations.after_rps IS
 'Audit of constructor-local counters/pending decision; never restore it as saved character state or send it in LOADCHAR/RELEASEMAIN. Relogin resets and existing local target objects retain their own counters.';
COMMENT ON VIEW game_compat."TRPSGAMECHART" IS
 'Full13-field source Map RPS projection; transient Control edits exist only in the native Map cache. No synthetic reward data or historical writes.';
REVOKE ALL ON game_compat.rps_release,game_compat."TRPSGAMECHART" FROM PUBLIC;
