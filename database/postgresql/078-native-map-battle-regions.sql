-- Phase A: written only. Original archives and migrations001--077 are unchanged.
CREATE SCHEMA battle_compat;
CREATE TABLE runtime_control.map_battle_catalog (
    world_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    routing_manifest text NOT NULL CHECK(routing_manifest ~ '^[0-9a-f]{64}$'),
    character_manifest text NOT NULL CHECK(character_manifest ~ '^[0-9a-f]{64}$'),
    content_manifest text NOT NULL CHECK(content_manifest ~ '^[0-9a-f]{64}$'),
    source_timezone text NOT NULL CHECK(length(source_timezone)>0),
    order_sha256 text NOT NULL CHECK(order_sha256 ~ '^[0-9a-f]{64}$'),
    region_rows bigint NOT NULL CHECK(region_rows>=0),
    occupation_rows bigint NOT NULL CHECK(occupation_rows>=0),
    published_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE runtime_control.map_battle_sources (
    world_id smallint NOT NULL REFERENCES runtime_control.map_battle_catalog(world_id),
    source_table text NOT NULL CHECK(source_table IN ('TBATTLEZONECHART','TLOCALTABLE','TCASTLETABLE',
        'TMISSIONTABLE','TSKYGARDENTABLE','TLOCALOCCUPYTABLE','TGUILDTABLE','TINDUNCHART')),
    row_count bigint NOT NULL CHECK(row_count>=0),
    value_sha256 text NOT NULL CHECK(value_sha256 ~ '^[0-9a-f]{64}$'),
    bytes_sha256 text CHECK(bytes_sha256 ~ '^[0-9a-f]{64}$'),
    PRIMARY KEY(world_id,source_table)
);
-- Guild IDs below are original DWORD identities, not a native guild-ID map.
-- Map has SELECT only until guild/occupation/hero mutation authority is ported.
CREATE TABLE app_world.battle_regions (
    world_id smallint NOT NULL REFERENCES runtime_control.map_battle_catalog(world_id),
    local_id integer NOT NULL CHECK(local_id BETWEEN 0 AND 65535),
    local_type smallint NOT NULL CHECK(local_type BETWEEN 1 AND 4),
    source_run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    source_row bigint NOT NULL CHECK(source_row>0),
    boot_order bigint NOT NULL CHECK(boot_order>0),
    country smallint NOT NULL CHECK(country BETWEEN 0 AND 255),
    source_guild_id bigint NOT NULL CHECK(source_guild_id BETWEEN 0 AND 4294967295),
    guild_name_bytes bytea,
    hero_bytes bytea NOT NULL CHECK(octet_length(hero_bytes)<=50),
    hero_second bigint NOT NULL CHECK(hero_second>=0),
    next_defend_second bigint NOT NULL CHECK(next_defend_second>=0),
    occupied_second bigint CHECK(occupied_second>=0),
    PRIMARY KEY(world_id,local_id),
    UNIQUE(world_id,local_type,boot_order),
    UNIQUE(world_id,local_type,source_row),
    CHECK(guild_name_bytes IS NULL OR octet_length(guild_name_bytes)<=50)
);
CREATE TABLE app_world.battle_occupation_days (
    world_id smallint NOT NULL,
    local_id integer NOT NULL,
    day smallint NOT NULL CHECK(day BETWEEN 1 AND 7),
    source_guild_id bigint NOT NULL CHECK(source_guild_id BETWEEN 0 AND 4294967295),
    occupy_type smallint NOT NULL CHECK(occupy_type BETWEEN 0 AND 255),
    source_row bigint NOT NULL CHECK(source_row>0),
    PRIMARY KEY(world_id,local_id,day),
    FOREIGN KEY(world_id,local_id) REFERENCES app_world.battle_regions
);
CREATE VIEW battle_compat.source_counts AS
    SELECT "_import_run_id" run_id,'TBATTLEZONECHART'::text source_table,count(*) row_count FROM legacy_game."TBATTLEZONECHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TLOCALTABLE',count(*) FROM legacy_game."TLOCALTABLE" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TCASTLETABLE',count(*) FROM legacy_game."TCASTLETABLE" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TMISSIONTABLE',count(*) FROM legacy_game."TMISSIONTABLE" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TSKYGARDENTABLE',count(*) FROM legacy_game."TSKYGARDENTABLE" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TLOCALOCCUPYTABLE',count(*) FROM legacy_game."TLOCALOCCUPYTABLE" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TGUILDTABLE',count(*) FROM legacy_game."TGUILDTABLE" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TINDUNCHART',count(*) FROM legacy_game."TINDUNCHART" GROUP BY 1;
CREATE VIEW battle_compat.catalog_release AS
    SELECT c.* FROM runtime_control.map_battle_catalog c
    JOIN world_compat.battle_release w ON w.world_id=c.world_id AND w.run_id=c.run_id
        AND w.manifest_sha256=c.manifest_sha256 AND w.source_timezone=c.source_timezone
    JOIN reconstruction.import_runs full_run ON full_run.id=c.run_id AND full_run.status='verified'
        AND full_run.manifest_sha256=c.manifest_sha256
    JOIN route_compat.catalog_release r ON r.manifest_sha256=c.routing_manifest AND r.status='verified'
    JOIN character_compat.catalog_release ch ON ch.manifest_sha256=c.character_manifest AND ch.status='verified'
    JOIN game_compat.catalog_release g ON g.manifest_sha256=c.content_manifest AND g.status='verified'
    WHERE (SELECT count(*) FROM reconstruction.import_runs dep
        WHERE dep.id IN (r.run_id,ch.run_id,g.run_id) AND dep.status='verified'
            AND dep.source_manifest->>'metadata_sha256'=full_run.source_manifest->>'metadata_sha256'
            AND dep.source_manifest->'backups' @> full_run.source_manifest->'backups'
            AND full_run.source_manifest->'backups' @> dep.source_manifest->'backups')=
        (SELECT count(DISTINCT id) FROM (VALUES(r.run_id),(ch.run_id),(g.run_id)) dependency(id))
        AND (SELECT count(*) FROM (VALUES
            (r.run_id,'TMAPCHART'),(r.run_id,'TUNITCHART'),(r.run_id,'TCHANNELCHART'),(r.run_id,'TSPAWNPOSCHART'),
            (ch.run_id,'TITEMCHART'),(ch.run_id,'TNPCCHART'),(ch.run_id,'TSKILLCHART'),(ch.run_id,'TSKILLDATA'),
            (g.run_id,'TMONSPAWNCHART')) dependency(run_id,source_table)
            JOIN reconstruction.checkpoints f ON f.run_id=c.run_id AND f.source_database='TGAME_RAGEZONE'
                AND f.source_schema='dbo' AND f.source_table=dependency.source_table
            JOIN reconstruction.checkpoints d ON d.run_id=dependency.run_id AND d.source_database=f.source_database
                AND d.source_schema=f.source_schema AND d.source_table=f.source_table
                AND d.row_count=f.row_count AND d.source_sha256=f.source_sha256 AND d.target_sha256=f.source_sha256
                AND f.target_sha256=f.source_sha256
                AND d.source_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256
                AND d.target_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256
                AND f.target_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256)=9
        AND (SELECT count(*) FROM runtime_control.map_battle_sources s
        JOIN reconstruction.checkpoints p ON p.run_id=c.run_id AND p.source_database='TGAME_RAGEZONE'
            AND p.source_schema='dbo' AND p.source_table=s.source_table AND p.row_count=s.row_count
            AND p.source_sha256=s.value_sha256 AND p.target_sha256=s.value_sha256
            AND p.source_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
            AND p.target_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
        LEFT JOIN battle_compat.source_counts n ON n.run_id=c.run_id AND n.source_table=s.source_table
        WHERE s.world_id=c.world_id AND COALESCE(n.row_count,0)=s.row_count)=8
        AND (SELECT count(*) FROM app_world.battle_regions b WHERE b.world_id=c.world_id)=c.region_rows
        AND (SELECT count(*) FROM app_world.battle_occupation_days b WHERE b.world_id=c.world_id)=c.occupation_rows;
CREATE VIEW battle_compat."TBATTLEZONECHART" AS
    SELECT c.world_id,z.*,encode(t.source_bytes,'hex') AS name_bytes
    FROM battle_compat.catalog_release c JOIN legacy_game."TBATTLEZONECHART" z ON z."_import_run_id"=c.run_id
    LEFT JOIN reconstruction.source_text_bytes t ON t.run_id=c.run_id AND t.source_database='TGAME_RAGEZONE'
        AND t.source_schema='dbo' AND t.source_table='TBATTLEZONECHART'
        AND t.source_row_number=z."_source_row_number" AND t.column_name='szName';
CREATE VIEW battle_compat."TINDUNCHART" AS
    SELECT c.world_id,s.* FROM battle_compat.catalog_release c
    JOIN legacy_game."TINDUNCHART" s ON s."_import_run_id"=c.run_id;
COMMENT ON TABLE app_world.battle_regions IS
 'Native boot projection of original mission/castle/SkyGarden/local rows with exact source query order, ANSI bytes and explicit DBTOTIME conversion. Source guild identities are provenance only until native guild authority exists. Process-local battle/status/map objects are initialized by the source-specific loader; this table does not implement battle effects.';
REVOKE ALL ON SCHEMA battle_compat FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA battle_compat FROM PUBLIC;
REVOKE ALL ON runtime_control.map_battle_catalog,runtime_control.map_battle_sources,
    app_world.battle_regions,app_world.battle_occupation_days FROM PUBLIC;
