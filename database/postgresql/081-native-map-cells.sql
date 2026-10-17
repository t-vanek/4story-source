-- Phase A: written only. No historical data or migration001--080 is changed.
CREATE TABLE runtime_control.map_cell_catalog (
    world_id smallint NOT NULL,
    server_id smallint NOT NULL,
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    routing_manifest text NOT NULL CHECK(routing_manifest ~ '^[0-9a-f]{64}$'),
    allocation_order_sha256 text NOT NULL CHECK(allocation_order_sha256 ~ '^[0-9a-f]{64}$'),
    order_sha256 text NOT NULL CHECK(order_sha256 ~ '^[0-9a-f]{64}$'),
    global_database_alias text NOT NULL CHECK(global_database_alias='TGLOBAL_GSP=TGLOBAL_RAGEZONE'),
    channel_rows bigint NOT NULL CHECK(channel_rows>=0),
    published_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(world_id,server_id),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_catalog
);
CREATE TABLE runtime_control.map_cell_sources (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    source_database text NOT NULL,source_table text NOT NULL,
    row_count bigint NOT NULL CHECK(row_count>=0),
    value_sha256 text NOT NULL CHECK(value_sha256 ~ '^[0-9a-f]{64}$'),
    bytes_sha256 text CHECK(bytes_sha256 ~ '^[0-9a-f]{64}$'),
    CHECK((source_database='TGLOBAL_RAGEZONE' AND source_table='TCHANNEL') OR
          (source_database='TGAME_RAGEZONE' AND source_table IN ('TMAPCHART','TUNITCHART','TCHANNELCHART','TSPAWNPOSCHART'))),
    PRIMARY KEY(world_id,server_id,source_database,source_table),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_cell_catalog
);
CREATE TABLE runtime_control.map_cell_channel_order (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    source_row bigint NOT NULL CHECK(source_row>0),
    query_order bigint NOT NULL CHECK(query_order>0),
    PRIMARY KEY(world_id,server_id,source_row),
    UNIQUE(world_id,server_id,query_order),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_cell_catalog
);
CREATE VIEW route_compat.cell_source_counts AS
    SELECT "_import_run_id" run_id,'TGAME_RAGEZONE'::text source_database,'TMAPCHART'::text source_table,count(*) row_count FROM legacy_game."TMAPCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TGAME_RAGEZONE','TUNITCHART',count(*) FROM legacy_game."TUNITCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TGAME_RAGEZONE','TCHANNELCHART',count(*) FROM legacy_game."TCHANNELCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TGAME_RAGEZONE','TSPAWNPOSCHART',count(*) FROM legacy_game."TSPAWNPOSCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TGLOBAL_RAGEZONE','TCHANNEL',count(*) FROM legacy_global."TCHANNEL" GROUP BY 1;
CREATE VIEW route_compat.cell_catalog_release AS
    SELECT c.* FROM runtime_control.map_cell_catalog c
    JOIN monster_compat.catalog_release m ON m.world_id=c.world_id AND m.server_id=c.server_id
        AND m.run_id=c.run_id AND m.manifest_sha256=c.manifest_sha256
        AND m.routing_manifest=c.routing_manifest AND m.order_sha256=c.allocation_order_sha256
    JOIN route_compat.catalog_release r ON r.manifest_sha256=c.routing_manifest AND r.status='verified'
    WHERE (SELECT count(*) FROM runtime_control.map_cell_sources s
        JOIN reconstruction.checkpoints p ON p.run_id=c.run_id AND p.source_schema='dbo'
            AND p.source_database=s.source_database AND p.source_table=s.source_table AND p.row_count=s.row_count
            AND p.source_sha256=s.value_sha256 AND p.target_sha256=s.value_sha256
            AND p.source_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
            AND p.target_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
        LEFT JOIN route_compat.cell_source_counts n ON n.run_id=c.run_id
            AND n.source_database=s.source_database AND n.source_table=s.source_table
        WHERE s.world_id=c.world_id AND s.server_id=c.server_id AND COALESCE(n.row_count,0)=s.row_count)=5
    AND (SELECT count(*) FROM runtime_control.map_cell_sources s
        JOIN reconstruction.checkpoints p ON p.run_id=r.run_id AND p.source_schema='dbo'
            AND p.source_database=s.source_database AND p.source_table=s.source_table AND p.row_count=s.row_count
            AND p.source_sha256=s.value_sha256 AND p.target_sha256=s.value_sha256
            AND p.source_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
            AND p.target_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
        WHERE s.world_id=c.world_id AND s.server_id=c.server_id AND s.source_database='TGAME_RAGEZONE')=4
    AND (SELECT count(*) FROM runtime_control.map_cell_channel_order o
        WHERE o.world_id=c.world_id AND o.server_id=c.server_id)=c.channel_rows
    AND c.channel_rows=(SELECT row_count FROM runtime_control.map_cell_sources s
        WHERE s.world_id=c.world_id AND s.server_id=c.server_id AND s.source_database='TGLOBAL_RAGEZONE' AND s.source_table='TCHANNEL');
-- TGAME dbo.TCHANNEL projects ALL groups from TGLOBAL_GSP. The explicit
-- publication alias resolves that backup naming discrepancy; no world filter.
CREATE VIEW route_compat.source_cell_channels AS
    SELECT c.world_id,c.server_id,o.query_order,s."bGroupID",s."bChannel"
    FROM route_compat.cell_catalog_release c JOIN runtime_control.map_cell_channel_order o
        ON o.world_id=c.world_id AND o.server_id=c.server_id
    JOIN legacy_global."TCHANNEL" s ON s."_import_run_id"=c.run_id AND s."_source_row_number"=o.source_row;
-- TGetServerID's exact four-column join. No group/world filter or arbitrary
-- LIMIT1: the C++ lookup refuses distinct owners for a needed source query.
CREATE VIEW route_compat.source_cell_owners AS
    SELECT s."wMapID",s."wUnitID",c."bLogChannel",s."bServerID"
    FROM route_compat."TSVRCHART" s JOIN route_compat."TCHANNELCHART" c
        ON s."bGroup"=c."bGroupID" AND s."wMapID"=c."wMapID"
        AND s."wUnitID"=c."wUnitID" AND s."bChannel"=c."bPhyChannel";
REVOKE ALL ON runtime_control.map_cell_catalog,runtime_control.map_cell_sources,
    runtime_control.map_cell_channel_order,route_compat.cell_source_counts,route_compat.cell_catalog_release,
    route_compat.source_cell_channels,route_compat.source_cell_owners FROM PUBLIC;
