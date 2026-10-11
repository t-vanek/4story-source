-- Native monetary portion of TGetPvPRecord/TGetMonthPvPoint and UsePvPoint.
-- Source signed INT bits are represented as unsigned DWORD values. Historical
-- rows stay in legacy_game; projection requires the explicit offline importer.
CREATE TABLE app_world.pvp_balances (
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 total bigint NOT NULL CHECK(total BETWEEN 0 AND 4294967295),
 available bigint NOT NULL CHECK(available BETWEEN 0 AND 4294967295),
 month bigint NOT NULL CHECK(month BETWEEN 0 AND 4294967295),
 PRIMARY KEY(world_id,char_id),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE"("bWorldID","dwCharID") ON DELETE CASCADE
);
COMMENT ON TABLE app_world.pvp_balances IS
 'Fresh-primary balance authority; missing row means the source procedures'' explicit zero outputs. Graph-backed characters retain the complete transfer checkpoint as authority. Only fenced native operations may spend; checkpoints cannot restore a prior balance. This is not the full PvP record/ranking subsystem.';
ALTER TABLE app_world.quest_operations
 ADD COLUMN before_pvp jsonb CHECK(before_pvp IS NULL OR jsonb_typeof(before_pvp)='array' AND jsonb_array_length(before_pvp)=3),
 ADD COLUMN after_pvp jsonb CHECK(after_pvp IS NULL OR jsonb_typeof(after_pvp)='array' AND jsonb_array_length(after_pvp)=3);
CREATE TABLE reconstruction.pvp_balance_imports (
 run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
 world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
 character_mapping_sha256 text NOT NULL CHECK(character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
 source_point_rows bigint NOT NULL CHECK(source_point_rows>=0),
 source_month_rows bigint NOT NULL CHECK(source_month_rows>=0),
 imported_characters bigint NOT NULL CHECK(imported_characters>=0),
 imported_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY(run_id,world_id)
);
REVOKE ALL ON app_world.pvp_balances,reconstruction.pvp_balance_imports FROM PUBLIC;
