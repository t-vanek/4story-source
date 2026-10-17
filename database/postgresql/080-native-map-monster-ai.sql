-- Phase A: written only. Source archives and migrations001--079 unchanged.
CREATE TABLE runtime_control.map_monster_ai_catalog (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    allocation_order_sha256 text NOT NULL CHECK(allocation_order_sha256 ~ '^[0-9a-f]{64}$'),
    order_sha256 text NOT NULL CHECK(order_sha256 ~ '^[0-9a-f]{64}$'),
    command_rows bigint NOT NULL CHECK(command_rows>=0),
    condition_rows bigint NOT NULL CHECK(condition_rows>=0),
    binding_rows bigint NOT NULL CHECK(binding_rows>=0),
    published_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(world_id,server_id),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_catalog
);
CREATE TABLE runtime_control.map_monster_ai_sources (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    source_table text NOT NULL CHECK(source_table IN ('TAICHART','TAICMDCHART','TAICONCHART')),
    row_count bigint NOT NULL CHECK(row_count>=0),
    value_sha256 text NOT NULL CHECK(value_sha256 ~ '^[0-9a-f]{64}$'),
    bytes_sha256 text CHECK(bytes_sha256 ~ '^[0-9a-f]{64}$'),
    PRIMARY KEY(world_id,server_id,source_table),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_ai_catalog
);
CREATE TABLE runtime_control.map_monster_ai_binding_order (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    source_row bigint NOT NULL CHECK(source_row>0),query_order bigint NOT NULL CHECK(query_order>0),
    PRIMARY KEY(world_id,server_id,source_row),UNIQUE(world_id,server_id,query_order),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_ai_catalog
);
CREATE TABLE runtime_control.map_monster_ai_condition_order (
    world_id smallint NOT NULL,server_id smallint NOT NULL,
    command_id bigint NOT NULL CHECK(command_id BETWEEN 0 AND 4294967295),
    source_row bigint NOT NULL CHECK(source_row>0),query_order bigint NOT NULL CHECK(query_order>0),
    PRIMARY KEY(world_id,server_id,command_id,source_row),UNIQUE(world_id,server_id,command_id,query_order),
    FOREIGN KEY(world_id,server_id) REFERENCES runtime_control.map_monster_ai_catalog
);
CREATE VIEW monster_compat.ai_source_counts AS
    SELECT "_import_run_id" run_id,'TAICHART'::text source_table,count(*) row_count FROM legacy_game."TAICHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TAICMDCHART',count(*) FROM legacy_game."TAICMDCHART" GROUP BY 1
    UNION ALL SELECT "_import_run_id",'TAICONCHART',count(*) FROM legacy_game."TAICONCHART" GROUP BY 1;
CREATE VIEW monster_compat.ai_release AS
    SELECT c.* FROM runtime_control.map_monster_ai_catalog c
    JOIN monster_compat.catalog_release a ON a.world_id=c.world_id AND a.server_id=c.server_id
        AND a.run_id=c.run_id AND a.manifest_sha256=c.manifest_sha256 AND a.order_sha256=c.allocation_order_sha256
    WHERE (SELECT count(*) FROM runtime_control.map_monster_ai_sources s
        JOIN reconstruction.checkpoints p ON p.run_id=c.run_id AND p.source_database='TGAME_RAGEZONE'
            AND p.source_schema='dbo' AND p.source_table=s.source_table AND p.row_count=s.row_count
            AND p.source_sha256=s.value_sha256 AND p.target_sha256=s.value_sha256
            AND p.source_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
            AND p.target_bytes_sha256 IS NOT DISTINCT FROM s.bytes_sha256
        LEFT JOIN monster_compat.ai_source_counts n ON n.run_id=c.run_id AND n.source_table=s.source_table
        WHERE s.world_id=c.world_id AND s.server_id=c.server_id AND COALESCE(n.row_count,0)=s.row_count)=3
        AND (SELECT count(*) FROM legacy_game."TAICMDCHART" s WHERE s."_import_run_id"=c.run_id)=c.command_rows
        AND (SELECT count(*) FROM legacy_game."TAICHART" s WHERE s."_import_run_id"=c.run_id)=c.binding_rows
        AND (SELECT count(*) FROM legacy_game."TAICONCHART" s WHERE s."_import_run_id"=c.run_id
            AND EXISTS(SELECT 1 FROM legacy_game."TAICMDCHART" m WHERE m."_import_run_id"=c.run_id AND m."dwCmdID"=s."dwCmdID"))=c.condition_rows
        AND (SELECT count(*) FROM runtime_control.map_monster_ai_binding_order o WHERE o.world_id=c.world_id AND o.server_id=c.server_id)=c.binding_rows
        AND (SELECT count(*) FROM runtime_control.map_monster_ai_condition_order o WHERE o.world_id=c.world_id AND o.server_id=c.server_id)=c.condition_rows;
CREATE VIEW monster_compat."TAICMDCHART" AS
    SELECT c.world_id,c.server_id,s.* FROM monster_compat.ai_release c
    JOIN legacy_game."TAICMDCHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat."TAICONCHART" AS
    SELECT c.world_id,c.server_id,o.query_order,s.* FROM monster_compat.ai_release c
    JOIN runtime_control.map_monster_ai_condition_order o USING(world_id,server_id)
    JOIN legacy_game."TAICONCHART" s ON s."_import_run_id"=c.run_id AND s."_source_row_number"=o.source_row
        AND (s."dwCmdID"::bigint & 4294967295)=o.command_id;
CREATE VIEW monster_compat."TAICHART" AS
    SELECT c.world_id,c.server_id,o.query_order,s.* FROM monster_compat.ai_release c
    JOIN runtime_control.map_monster_ai_binding_order o USING(world_id,server_id)
    JOIN legacy_game."TAICHART" s ON s."_import_run_id"=c.run_id AND s."_source_row_number"=o.source_row;
COMMENT ON VIEW monster_compat.ai_release IS
 'Exact original AI command/condition/binding producer bound to079; source query-order input does not establish client compatibility or full runtime AI support.';
REVOKE ALL ON monster_compat.ai_source_counts,monster_compat.ai_release,monster_compat."TAICMDCHART",
    monster_compat."TAICONCHART",monster_compat."TAICHART" FROM PUBLIC;
REVOKE ALL ON runtime_control.map_monster_ai_catalog,runtime_control.map_monster_ai_sources,
    runtime_control.map_monster_ai_binding_order,runtime_control.map_monster_ai_condition_order FROM PUBLIC;
