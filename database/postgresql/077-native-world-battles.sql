-- Phase A only. Publish complete historical battle/time charts, never rewrite
-- them to repair undefined source mission initialization or implicit row order.
CREATE TABLE runtime_control.world_battle_catalog (
    world_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    battle_sha256 text NOT NULL CHECK(battle_sha256 ~ '^[0-9a-f]{64}$'),
    custom_sha256 text NOT NULL CHECK(custom_sha256 ~ '^[0-9a-f]{64}$'),
    battle_rows bigint NOT NULL CHECK(battle_rows=5),
    custom_rows bigint NOT NULL CHECK(custom_rows=3),
    source_timezone text NOT NULL CHECK(length(source_timezone)>0),
    custom_order jsonb NOT NULL CHECK(jsonb_typeof(custom_order)='array' AND jsonb_array_length(custom_order)=3),
    order_policy text NOT NULL CHECK(order_policy IN ('single-mission-order-independent','explicit-source-order-profile')),
    order_sha256 text NOT NULL CHECK(order_sha256 ~ '^[0-9a-f]{64}$'),
    published_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE VIEW world_compat.battle_release AS
    SELECT c.* FROM runtime_control.world_battle_catalog c
    JOIN reconstruction.import_runs r ON r.id=c.run_id AND r.manifest_sha256=c.manifest_sha256 AND r.status='verified'
    JOIN world_compat.rps_release p ON p.world_id=c.world_id AND p.run_id=c.run_id
        AND p.manifest_sha256=c.manifest_sha256 AND p.source_timezone=c.source_timezone
    JOIN reconstruction.checkpoints b ON b.run_id=c.run_id AND b.source_database='TGAME_RAGEZONE'
        AND b.source_schema='dbo' AND b.source_table='TBATTLETIMECHART' AND b.row_count=c.battle_rows
        AND b.source_sha256=c.battle_sha256 AND b.target_sha256=c.battle_sha256
    JOIN reconstruction.checkpoints t ON t.run_id=c.run_id AND t.source_database='TGAME_RAGEZONE'
        AND t.source_schema='dbo' AND t.source_table='TCUSTOMTIMECHART' AND t.row_count=c.custom_rows
        AND t.source_sha256=c.custom_sha256 AND t.target_sha256=c.custom_sha256
    WHERE (SELECT count(*) FROM legacy_game."TBATTLETIMECHART" s WHERE s."_import_run_id"=c.run_id)=c.battle_rows
        AND (SELECT count(*) FROM legacy_game."TCUSTOMTIMECHART" s WHERE s."_import_run_id"=c.run_id)=c.custom_rows;
CREATE VIEW world_compat."TBATTLETIMECHART" AS
    SELECT c.world_id,s.* FROM legacy_game."TBATTLETIMECHART" s
    JOIN world_compat.battle_release c ON c.run_id=s."_import_run_id";
CREATE VIEW world_compat."TCUSTOMTIMECHART" AS
    SELECT c.world_id,o.ordinality AS order_index,s.* FROM world_compat.battle_release c
    CROSS JOIN LATERAL jsonb_array_elements_text(c.custom_order) WITH ORDINALITY o(source_row,ordinality)
    JOIN legacy_game."TCUSTOMTIMECHART" s ON s."_import_run_id"=c.run_id AND s."_source_row_number"=o.source_row::bigint;
COMMENT ON TABLE runtime_control.world_battle_catalog IS
 'Original World battle/time chart publication and explicit source calendar/order profile. Schedules/status/running/left are source process-local state, reconstructed on startup, not saved character or invented zero-success data. Multiple mission rows require an explicit order profile; original SQL has no ORDER BY.';
REVOKE ALL ON runtime_control.world_battle_catalog,world_compat.battle_release,
    world_compat."TBATTLETIMECHART",world_compat."TCUSTOMTIMECHART" FROM PUBLIC;
