-- Phase A: written only; original archives and migrations001--078 unchanged.
CREATE SCHEMA monster_compat;
CREATE TABLE runtime_control.map_monster_catalog (
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    server_id smallint NOT NULL CHECK(server_id BETWEEN 0 AND 255),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    order_sha256 text NOT NULL CHECK(order_sha256 ~ '^[0-9a-f]{64}$'),
    routing_manifest text NOT NULL CHECK(routing_manifest ~ '^[0-9a-f]{64}$'),
    character_manifest text NOT NULL CHECK(character_manifest ~ '^[0-9a-f]{64}$'),
    content_manifest text NOT NULL CHECK(content_manifest ~ '^[0-9a-f]{64}$'),
    spawn_rows bigint NOT NULL CHECK(spawn_rows>=0),
    candidate_rows bigint NOT NULL CHECK(candidate_rows>=0),
    party_rows bigint NOT NULL CHECK(party_rows>=0),
    published_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(world_id,server_id)
);
CREATE TABLE runtime_control.map_monster_sources (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    source_table text NOT NULL CHECK(source_table IN ('TMONSPAWNCHART','TMAPMONCHART','TMONSTERCHART',
        'TMONATTRCHART','TCLASSCHART','TRACECHART','TLEVELCHART')),
    row_count bigint NOT NULL CHECK(row_count>=0),
    value_sha256 text NOT NULL CHECK(value_sha256 ~ '^[0-9a-f]{64}$'),
    bytes_sha256 text CHECK(bytes_sha256 ~ '^[0-9a-f]{64}$'),
    PRIMARY KEY(world_id,server_id,source_table),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_catalog
);
CREATE TABLE runtime_control.map_monster_spawn_order (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    source_row bigint NOT NULL CHECK(source_row>0),
    query_order bigint NOT NULL CHECK(query_order>0),
    PRIMARY KEY(world_id,server_id,source_row),
    UNIQUE(world_id,server_id,query_order),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_catalog
);
CREATE TABLE runtime_control.map_monster_candidate_order (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    spawn_id integer NOT NULL CHECK(spawn_id BETWEEN 0 AND 65535),
    source_row bigint NOT NULL CHECK(source_row>0),
    query_order bigint NOT NULL CHECK(query_order>0),
    PRIMARY KEY(world_id,server_id,spawn_id,source_row),
    UNIQUE(world_id,server_id,spawn_id,query_order),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_catalog
);
CREATE TABLE runtime_control.map_monster_party_order (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    spawn_id integer NOT NULL CHECK(spawn_id BETWEEN 0 AND 65535),
    source_row bigint NOT NULL CHECK(source_row>0),
    query_order bigint NOT NULL CHECK(query_order>0),
    PRIMARY KEY(world_id,server_id,spawn_id,source_row),
    UNIQUE(world_id,server_id,spawn_id,query_order),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_catalog
);
CREATE VIEW monster_compat.source_counts AS
    SELECT "_import_run_id" run_id,'TMONSPAWNCHART'::text source_table,count(*) row_count FROM legacy_game."TMONSPAWNCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TMAPMONCHART',count(*) FROM legacy_game."TMAPMONCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TMONSTERCHART',count(*) FROM legacy_game."TMONSTERCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TMONATTRCHART',count(*) FROM legacy_game."TMONATTRCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TCLASSCHART',count(*) FROM legacy_game."TCLASSCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TRACECHART',count(*) FROM legacy_game."TRACECHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TLEVELCHART',count(*) FROM legacy_game."TLEVELCHART" GROUP BY 1;
CREATE VIEW monster_compat.catalog_release AS
    SELECT c.* FROM runtime_control.map_monster_catalog c
    JOIN battle_compat.catalog_release b ON b.world_id=c.world_id AND b.run_id=c.run_id
        AND b.manifest_sha256=c.manifest_sha256 AND b.routing_manifest=c.routing_manifest
        AND b.character_manifest=c.character_manifest AND b.content_manifest=c.content_manifest
    JOIN reconstruction.import_runs full_run ON full_run.id=c.run_id AND full_run.status='verified'
        AND full_run.manifest_sha256=c.manifest_sha256
    JOIN game_compat.catalog_release g ON g.manifest_sha256=c.content_manifest AND g.status='verified'
    JOIN character_compat.catalog_release ch ON ch.manifest_sha256=c.character_manifest AND ch.status='verified'
    WHERE (SELECT count(*) FROM runtime_control.map_monster_sources s
        JOIN reconstruction.checkpoints p ON p.run_id=c.run_id AND p.source_database='TGAME_RAGEZONE'
            AND p.source_schema='dbo' AND p.source_table=s.source_table AND p.row_count=s.row_count
            AND p.source_sha256=s.value_sha256 AND p.target_sha256=s.value_sha256
            AND p.source_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
            AND p.target_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
        LEFT JOIN monster_compat.source_counts n ON n.run_id=c.run_id AND n.source_table=s.source_table
        WHERE s.world_id=c.world_id AND s.server_id=c.server_id AND COALESCE(n.row_count,0)=s.row_count)=7
        AND (SELECT count(*) FROM (VALUES
            (g.run_id,'TMONSPAWNCHART'),(g.run_id,'TMAPMONCHART'),(g.run_id,'TMONSTERCHART'),(g.run_id,'TMONATTRCHART'),
            (g.run_id,'TCLASSCHART'),(g.run_id,'TRACECHART'),(ch.run_id,'TLEVELCHART')) d(run_id,source_table)
        JOIN reconstruction.checkpoints f ON f.run_id=c.run_id AND f.source_database='TGAME_RAGEZONE'
            AND f.source_schema='dbo' AND f.source_table=d.source_table
        JOIN reconstruction.checkpoints a ON a.run_id=d.run_id AND a.source_database=f.source_database
            AND a.source_schema=f.source_schema AND a.source_table=f.source_table
            AND a.row_count=f.row_count AND a.source_sha256=f.source_sha256 AND a.target_sha256=f.source_sha256
            AND a.source_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256
            AND a.target_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256)=7
        AND (SELECT count(*) FROM runtime_control.map_monster_spawn_order o WHERE o.world_id=c.world_id AND o.server_id=c.server_id)=c.spawn_rows
        AND (SELECT count(*) FROM runtime_control.map_monster_candidate_order o WHERE o.world_id=c.world_id AND o.server_id=c.server_id)=c.candidate_rows
        AND (SELECT count(*) FROM runtime_control.map_monster_party_order o WHERE o.world_id=c.world_id AND o.server_id=c.server_id)=c.party_rows;
CREATE VIEW monster_compat.source_server_spawns AS
    SELECT c.world_id,c.server_id,s.* FROM monster_compat.catalog_release c
    JOIN legacy_game."TMONSPAWNCHART" s ON s."_import_run_id"=c.run_id
    WHERE s."wMapID" IN (SELECT "wMapID" FROM route_compat.source_map_regions WHERE "bServerID"=c.server_id)
        AND trunc((s."fPosZ"/1024::real)::double precision)::smallint::integer*256+
            trunc((s."fPosX"/1024::real)::double precision)::smallint::integer IN
            (SELECT "wUnitID" FROM route_compat.source_map_regions WHERE "bServerID"=c.server_id);
CREATE VIEW monster_compat."TMONSPAWNCHART" AS
    SELECT c.world_id,c.server_id,o.query_order,s.* FROM monster_compat.catalog_release c
    JOIN runtime_control.map_monster_spawn_order o USING(world_id,server_id)
    JOIN legacy_game."TMONSPAWNCHART" s ON s."_import_run_id"=c.run_id AND s."_source_row_number"=o.source_row
    WHERE EXISTS(SELECT 1 FROM monster_compat.source_server_spawns f WHERE f.world_id=c.world_id AND f.server_id=c.server_id
        AND f."_source_row_number"=s."_source_row_number");
CREATE VIEW monster_compat."TMAPMONCHART" AS
    SELECT c.world_id,c.server_id,o.query_order,s.* FROM monster_compat.catalog_release c
    JOIN runtime_control.map_monster_candidate_order o USING(world_id,server_id)
    JOIN legacy_game."TMAPMONCHART" s ON s."_import_run_id"=c.run_id AND s."_source_row_number"=o.source_row
        AND (s."wSpawnID"::integer & 65535)=o.spawn_id;
CREATE VIEW monster_compat."TMONPARTY" AS
    SELECT c.world_id,c.server_id,o.spawn_id,o.query_order,s."wID" FROM monster_compat.catalog_release c
    JOIN runtime_control.map_monster_party_order o USING(world_id,server_id)
    JOIN monster_compat.source_server_spawns p ON p.world_id=c.world_id AND p.server_id=c.server_id
        AND (p."wID"::integer & 65535)=o.spawn_id AND p."wPartyID"<>0
    JOIN legacy_game."TMONSPAWNCHART" s ON s."_import_run_id"=c.run_id AND s."_source_row_number"=o.source_row
        AND s."wPartyID"=p."wPartyID";
CREATE VIEW monster_compat."TMONSTERCHART" AS
    SELECT c.world_id,c.server_id,s.* FROM monster_compat.catalog_release c
    JOIN legacy_game."TMONSTERCHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat."TMONATTRCHART" AS
    SELECT c.world_id,c.server_id,s.* FROM monster_compat.catalog_release c
    JOIN legacy_game."TMONATTRCHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat."TCLASSCHART" AS
    SELECT c.world_id,c.server_id,s."bClassID" FROM monster_compat.catalog_release c
    JOIN legacy_game."TCLASSCHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat."TRACECHART" AS
    SELECT c.world_id,c.server_id,s."bRaceID" FROM monster_compat.catalog_release c
    JOIN legacy_game."TRACECHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat."TLEVELCHART" AS
    SELECT c.world_id,c.server_id,s."bLevel" FROM monster_compat.catalog_release c
    JOIN legacy_game."TLEVELCHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat.allocation_counts AS
    SELECT c.world_id,c.server_id,
        (SELECT count(*) FROM monster_compat.source_server_spawns f
            WHERE f.world_id=c.world_id AND f.server_id=c.server_id) AS spawn_rows,
        (SELECT count(*) FROM legacy_game."TMAPMONCHART" m WHERE m."_import_run_id"=c.run_id
            AND EXISTS(SELECT 1 FROM monster_compat.source_server_spawns f
                WHERE f.world_id=c.world_id AND f.server_id=c.server_id AND f."wID"=m."wSpawnID")) AS candidate_rows,
        (SELECT count(*) FROM monster_compat.source_server_spawns f
            JOIN legacy_game."TMONSPAWNCHART" s ON s."_import_run_id"=c.run_id AND s."wPartyID"=f."wPartyID"
            WHERE f.world_id=c.world_id AND f.server_id=c.server_id AND f."wPartyID"<>0) AS party_rows
    FROM monster_compat.catalog_release c;
COMMENT ON SCHEMA monster_compat IS
 'Source Map spawn/template/attribute allocation and original query-order provenance only. No publication of live monsters, AI success, client-host authority or completed monster lifecycle is implied.';
REVOKE ALL ON SCHEMA monster_compat FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA monster_compat FROM PUBLIC;
REVOKE ALL ON runtime_control.map_monster_catalog,runtime_control.map_monster_sources,
    runtime_control.map_monster_spawn_order,runtime_control.map_monster_candidate_order,
    runtime_control.map_monster_party_order FROM PUBLIC;
