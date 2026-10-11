-- Native replacement of TITEMUSEDTABLE / TTEMPITEMUSEDTABLE / TSaveItemUsed.
-- The immutable archive retains signed SQL Server bit representations. Runtime
-- uses WORD group IDs and DWORD remaining milliseconds as nonnegative values.
CREATE TABLE app_world.item_cooldowns (
    world_id smallint NOT NULL,
    char_id integer NOT NULL,
    delay_group integer NOT NULL CHECK(delay_group BETWEEN 0 AND 65535),
    remaining bigint NOT NULL CHECK(remaining BETWEEN 0 AND 4294967295),
    PRIMARY KEY(world_id,char_id,delay_group),
    FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE FUNCTION app_world.map_item_cooldown_state(p_world smallint,p_char integer)
RETURNS jsonb LANGUAGE sql STABLE AS $$
    SELECT COALESCE(jsonb_agg(jsonb_build_array(delay_group,remaining) ORDER BY delay_group),'[]'::jsonb)
    FROM app_world.item_cooldowns WHERE world_id=p_world AND char_id=p_char
$$;
ALTER TABLE app_world.map_checkpoints ADD COLUMN item_cooldown_state jsonb NOT NULL DEFAULT '[]'::jsonb
    CHECK(jsonb_typeof(item_cooldown_state)='array');
CREATE OR REPLACE FUNCTION app_world.map_checkpoint_matches(p app_world.map_checkpoints)
RETURNS boolean LANGUAGE sql STABLE AS $$
    SELECT COALESCE(p.core_state=app_world.map_core_state(p.world_id,p.char_id)
        AND p.recovery_contract IN (1,2,3,4)
        AND (p.recovery_contract NOT IN (3,4) OR p.skill_state=app_world.map_skill_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR COALESCE(p.maintain_state,'[]'::jsonb)=app_world.map_maintain_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR COALESCE(p.quest_state,'[]'::jsonb)=app_world.map_quest_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR p.during_state=app_world.map_exp_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR p.item_cooldown_state=app_world.map_item_cooldown_state(p.world_id,p.char_id)),false)
$$;
CREATE TABLE reconstruction.item_cooldown_imports (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    character_mapping_sha256 text NOT NULL CHECK(character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    imported_rows bigint NOT NULL CHECK(imported_rows>=0),
    imported_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(run_id,world_id)
);
REVOKE ALL ON app_world.item_cooldowns,reconstruction.item_cooldown_imports FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_item_cooldown_state(smallint,integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_checkpoint_matches(app_world.map_checkpoints) FROM PUBLIC;
COMMENT ON TABLE app_world.item_cooldowns IS
 'Source remaining item reuse, never an absolute process tick. Core checkpoints only decrease existing timers; native ITEMUSE arms them with item/resource/EXP changes in one fenced quest transaction. Full transfer graph supersedes these rows after handoff. Offline time does not reduce remaining reuse. LOADCHAR keeps original tutorial and one-hour filters; archival rows are never changed.';
