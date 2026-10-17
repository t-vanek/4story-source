-- Phase A: written only. Exact CTBLSkillData query results; no source data changes.
CREATE TABLE runtime_control.map_skill_data_catalog (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    allocation_order_sha256 text NOT NULL CHECK(allocation_order_sha256 ~ '^[0-9a-f]{64}$'),
    character_manifest text NOT NULL CHECK(character_manifest ~ '^[0-9a-f]{64}$'),
    order_sha256 text NOT NULL CHECK(order_sha256 ~ '^[0-9a-f]{64}$'),
    source_revision text NOT NULL CHECK(source_revision='c77f79017d7940c1500ba36a5dbf8bda6fefb67c'),
    source_rows bigint NOT NULL CHECK(source_rows>=0),
    source_value_sha256 text NOT NULL CHECK(source_value_sha256 ~ '^[0-9a-f]{64}$'),
    source_bytes_sha256 text CHECK(source_bytes_sha256 ~ '^[0-9a-f]{64}$'),
    premium_rows bigint NOT NULL CHECK(premium_rows>=0),
    premium_value_sha256 text NOT NULL CHECK(premium_value_sha256 ~ '^[0-9a-f]{64}$'),
    premium_bytes_sha256 text CHECK(premium_bytes_sha256 ~ '^[0-9a-f]{64}$'),
    skill_queries bigint NOT NULL CHECK(skill_queries BETWEEN 0 AND 65536),
    data_rows bigint NOT NULL CHECK(data_rows>=0),
    published_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(world_id,server_id),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_catalog
);
CREATE TABLE runtime_control.map_skill_data_queries (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    skill_id integer NOT NULL CHECK(skill_id BETWEEN 0 AND 65535),
    data_rows bigint NOT NULL CHECK(data_rows>=0),
    PRIMARY KEY(world_id,server_id,skill_id),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_skill_data_catalog
);
CREATE TABLE runtime_control.map_skill_data_order (
    world_id smallint NOT NULL,server_id smallint NOT NULL,skill_id integer NOT NULL,
    source_row bigint NOT NULL CHECK(source_row>0),query_order bigint NOT NULL CHECK(query_order>0),
    PRIMARY KEY(world_id,server_id,source_row),UNIQUE(world_id,server_id,skill_id,query_order),
    FOREIGN KEY(world_id,server_id,skill_id) REFERENCES runtime_control.map_skill_data_queries
);
CREATE VIEW monster_compat.skill_data_release AS
    WITH skills AS (
        SELECT "_import_run_id" run_id,("wID"::integer & 65535) skill_id FROM legacy_game."TSKILLCHART"
    ), ordered_counts AS (
        SELECT world_id,server_id,skill_id,count(*) data_rows,max(query_order) last_order
        FROM runtime_control.map_skill_data_order GROUP BY world_id,server_id,skill_id
    )
    SELECT c.* FROM runtime_control.map_skill_data_catalog c
    JOIN monster_compat.skill_release a ON a.world_id=c.world_id AND a.server_id=c.server_id
        AND a.run_id=c.run_id AND a.manifest_sha256=c.manifest_sha256
        AND a.order_sha256=c.allocation_order_sha256 AND a.character_manifest=c.character_manifest
    JOIN character_compat.catalog_release ch ON ch.manifest_sha256=c.character_manifest AND ch.status='verified'
    JOIN reconstruction.checkpoints f ON f.run_id=c.run_id AND f.source_database='TGAME_RAGEZONE'
        AND f.source_schema='dbo' AND f.source_table='TSKILLDATA' AND f.row_count=c.source_rows
        AND f.source_sha256=c.source_value_sha256 AND f.target_sha256=c.source_value_sha256
        AND f.source_bytes_sha256 IS NOT DISTINCT FROM c.source_bytes_sha256
        AND f.target_bytes_sha256 IS NOT DISTINCT FROM c.source_bytes_sha256
    JOIN reconstruction.checkpoints p ON p.run_id=ch.run_id AND p.source_database=f.source_database
        AND p.source_schema=f.source_schema AND p.source_table=f.source_table
        AND p.row_count=f.row_count AND p.source_sha256=f.source_sha256 AND p.target_sha256=f.source_sha256
        AND p.source_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256
        AND p.target_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256
    JOIN reconstruction.checkpoints premium ON premium.run_id=c.run_id AND premium.source_database='TGAME_RAGEZONE'
        AND premium.source_schema='dbo' AND premium.source_table='TPREMIUMSKILLCHART' AND premium.row_count=c.premium_rows
        AND premium.source_sha256=c.premium_value_sha256 AND premium.target_sha256=c.premium_value_sha256
        AND premium.source_bytes_sha256 IS NOT DISTINCT FROM c.premium_bytes_sha256
        AND premium.target_bytes_sha256 IS NOT DISTINCT FROM c.premium_bytes_sha256
    WHERE (SELECT count(*) FROM legacy_game."TSKILLDATA" d WHERE d."_import_run_id"=c.run_id)=c.source_rows
        AND (SELECT count(*) FROM legacy_game."TPREMIUMSKILLCHART" p WHERE p."_import_run_id"=c.run_id)=c.premium_rows
        AND (SELECT count(*) FROM skills s WHERE s.run_id=c.run_id)=c.skill_queries
        AND NOT EXISTS(SELECT 1 FROM skills s WHERE s.run_id=c.run_id GROUP BY s.skill_id HAVING count(*)<>1)
        AND (SELECT count(*) FROM runtime_control.map_skill_data_queries q
            WHERE q.world_id=c.world_id AND q.server_id=c.server_id)=c.skill_queries
        AND NOT EXISTS(SELECT 1 FROM runtime_control.map_skill_data_queries q
            LEFT JOIN skills s ON s.run_id=c.run_id AND s.skill_id=q.skill_id
            LEFT JOIN ordered_counts n ON n.world_id=q.world_id AND n.server_id=q.server_id AND n.skill_id=q.skill_id
            WHERE q.world_id=c.world_id AND q.server_id=c.server_id AND
                (s.skill_id IS NULL OR COALESCE(n.data_rows,0)<>q.data_rows OR COALESCE(n.last_order,0)<>q.data_rows))
        AND (SELECT count(*) FROM legacy_game."TSKILLDATA" d JOIN skills s
            ON s.run_id=d."_import_run_id" AND s.skill_id=(d."wSkillID"::integer & 65535)
            WHERE d."_import_run_id"=c.run_id)=c.data_rows
        AND (SELECT count(*) FROM runtime_control.map_skill_data_order o
            WHERE o.world_id=c.world_id AND o.server_id=c.server_id)=c.data_rows
        AND (SELECT count(*) FROM runtime_control.map_skill_data_order o JOIN legacy_game."TSKILLDATA" d
            ON d."_import_run_id"=c.run_id AND d."_source_row_number"=o.source_row
                AND (d."wSkillID"::integer & 65535)=o.skill_id
            WHERE o.world_id=c.world_id AND o.server_id=c.server_id)=c.data_rows;
CREATE VIEW monster_compat.skill_data_queries AS
    SELECT q.* FROM monster_compat.skill_data_release c
    JOIN runtime_control.map_skill_data_queries q USING(world_id,server_id);
CREATE VIEW monster_compat.skill_data AS
    SELECT c.world_id,c.server_id,o.skill_id,o.query_order,d.* FROM monster_compat.skill_data_release c
    JOIN runtime_control.map_skill_data_order o USING(world_id,server_id)
    JOIN legacy_game."TSKILLDATA" d ON d."_import_run_id"=c.run_id AND d."_source_row_number"=o.source_row
        AND (d."wSkillID"::integer & 65535)=o.skill_id;
CREATE VIEW monster_compat.premium_skill_removals AS
    SELECT c.world_id,c.server_id,p.* FROM monster_compat.skill_data_release c
    JOIN legacy_game."TPREMIUMSKILLCHART" p ON p."_import_run_id"=c.run_id;
COMMENT ON VIEW monster_compat.skill_data_release IS
 'Exact full/active TSKILLDATA equality plus complete per-existing-skill query order, including empty results. Archive ordinals are identities, never inferred result order; orphan rows remain archived and are not queried by source LoadData. Publication is not client/runtime verification.';
REVOKE ALL ON runtime_control.map_skill_data_catalog,runtime_control.map_skill_data_queries,
    runtime_control.map_skill_data_order,monster_compat.skill_data_release,
    monster_compat.skill_data_queries,monster_compat.skill_data,monster_compat.premium_skill_removals FROM PUBLIC;
