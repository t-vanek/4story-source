-- Source TEXPITEMTABLE / TTEMPEXPITEMTABLE / TSaveCharDataStart/End.
-- This is a new runtime projection, not a rewrite of either historical backup.
-- Source has no per-character unique key; ambiguous multi-row source slots are
-- retained in the archive and rejected explicitly by the offline import tool.
CREATE TABLE app_world.exp_entitlements (
    world_id smallint NOT NULL,
    char_id integer NOT NULL,
    item_id integer NOT NULL CHECK(item_id BETWEEN 0 AND 65535),
    item_type smallint NOT NULL CHECK(item_type BETWEEN 0 AND 255),
    remaining bigint NOT NULL CHECK(remaining BETWEEN 0 AND 4294967295),
    expires bigint NOT NULL,
    PRIMARY KEY(world_id,char_id),
    FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
-- Account entitlements have a different owner from EXP items. Preserve that
-- distinction even before premium effects, pets and daily PCBang are ported.
-- The current runtime refuses these accounts; it never deletes their rows.
CREATE TABLE app_global.during_items (
    user_id integer NOT NULL REFERENCES app_global."TACCOUNT_PW"("dwUserID") ON DELETE CASCADE,
    item_id integer NOT NULL CHECK(item_id BETWEEN 0 AND 65535),
    item_type smallint NOT NULL CHECK(item_type BETWEEN 0 AND 255),
    remaining bigint NOT NULL CHECK(remaining BETWEEN 0 AND 4294967295),
    expires bigint NOT NULL,
    PRIMARY KEY(user_id,item_id)
);
CREATE FUNCTION app_world.map_exp_state(p_world smallint,p_char integer)
RETURNS jsonb LANGUAGE sql STABLE AS $$
    SELECT COALESCE(jsonb_agg(jsonb_build_array(item_id,item_type,remaining,expires)), '[]'::jsonb)
    FROM app_world.exp_entitlements WHERE world_id=p_world AND char_id=p_char
$$;
ALTER TABLE app_world.map_checkpoints ADD COLUMN during_state jsonb NOT NULL DEFAULT '[]'::jsonb
    CHECK(jsonb_typeof(during_state)='array' AND jsonb_array_length(during_state)<=1);
CREATE OR REPLACE FUNCTION app_world.map_checkpoint_matches(p app_world.map_checkpoints)
RETURNS boolean LANGUAGE sql STABLE AS $$
    SELECT COALESCE(p.core_state=app_world.map_core_state(p.world_id,p.char_id)
        AND p.recovery_contract IN (1,2,3,4)
        AND (p.recovery_contract NOT IN (3,4) OR p.skill_state=app_world.map_skill_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR COALESCE(p.maintain_state,'[]'::jsonb)=app_world.map_maintain_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR COALESCE(p.quest_state,'[]'::jsonb)=app_world.map_quest_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR p.during_state=app_world.map_exp_state(p.world_id,p.char_id)),false)
$$;
CREATE TABLE reconstruction.during_item_imports (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    character_mapping_sha256 text NOT NULL CHECK(character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    account_mapping_sha256 text NOT NULL CHECK(account_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    source_timezone text NOT NULL,
    exp_items bigint NOT NULL CHECK(exp_items>=0),
    account_items bigint NOT NULL CHECK(account_items>=0),
    imported_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(run_id,world_id)
);
REVOKE ALL ON app_world.exp_entitlements,app_global.during_items,reconstruction.during_item_imports FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_exp_state(smallint,integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_checkpoint_matches(app_world.map_checkpoints) FROM PUBLIC;
COMMENT ON TABLE app_world.exp_entitlements IS
 'Original four-field EXP slot: unsigned item/type/seconds and explicit calendar epoch. Fresh primaries use these rows; after transfer the complete graph is authoritative. Mutations share the owner/account locks and quest ledger. A core save cannot overwrite this state. Historical naive smalldatetime needs an explicit source zone at import.';
