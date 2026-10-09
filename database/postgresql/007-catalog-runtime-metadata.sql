-- Read-only publication metadata for a least-privilege native C++ catalog reader.
-- No changes to historical snapshots or migrations 001-006.
CREATE VIEW game_compat.catalog_release AS
SELECT a.run_id, r.manifest_sha256, r.status
FROM runtime_control.active_catalog a
JOIN reconstruction.import_runs r ON r.id = a.run_id
WHERE a.singleton;

CREATE VIEW game_compat.catalog_source_tables AS
SELECT c.source_table, c.row_count, c.source_sha256, c.target_sha256,
       c.source_bytes_sha256, c.target_bytes_sha256
FROM reconstruction.checkpoints c
JOIN runtime_control.active_catalog a ON a.singleton AND a.run_id = c.run_id
WHERE c.source_database = 'TGAME_RAGEZONE' AND c.source_schema = 'dbo';

CREATE VIEW game_compat.catalog_missing_monster_attributes AS
SELECT monster_id, attribute_id, level
FROM runtime_control.missing_monster_attributes;

COMMENT ON VIEW game_compat.catalog_release IS
  'Read with all chart loads in one REPEATABLE READ READ ONLY transaction; caches remain fixed until restart';
COMMENT ON VIEW game_compat.catalog_source_tables IS
  'Publication provenance and source counts only; exposes no original row payloads';
REVOKE ALL ON game_compat.catalog_release, game_compat.catalog_source_tables,
    game_compat.catalog_missing_monster_attributes FROM PUBLIC;
