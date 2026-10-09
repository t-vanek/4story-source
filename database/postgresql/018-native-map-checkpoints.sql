-- Native runtime recovery contract v1: atomic core checkpoints. No historical rows.
-- One latest receipt per character, retained after logout/recovery for diagnosis
-- and confirmation of a commit whose network response was lost.
CREATE FUNCTION app_world.map_core_state(p_world smallint,p_char integer)
RETURNS jsonb LANGUAGE sql STABLE AS $$
 SELECT jsonb_build_array("bLevel","dwEXP","dwHP","dwMP","dwGold","dwSilver","dwCooper",
  "wSkillPoint","dwRegion","wMapID","wSpawnID","wLastSpawnID","dwLastDestination",
  "wTemptedMon","bAftermath","bStartAct","fPosX","fPosY","fPosZ","wDIR",
  "bStatLevel","bStatPoint","dwStatExp")
 FROM app_world."TCHARTABLE" WHERE "bWorldID"=p_world AND "dwCharID"=p_char
$$;
REVOKE ALL ON FUNCTION app_world.map_core_state(smallint,integer) FROM PUBLIC;
CREATE TABLE app_world.map_checkpoints (
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 user_id integer NOT NULL,
 server_id smallint NOT NULL CHECK(server_id BETWEEN 0 AND 255),
 session_key integer NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 revision bigint NOT NULL CHECK(revision>=0),
 fingerprint text NOT NULL CHECK(fingerprint ~ '^[0-9a-f]{64}$'),
 recovery_contract smallint NOT NULL CHECK(recovery_contract=1),
 core_state jsonb NOT NULL,
 saved_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 outcome text NOT NULL CHECK(outcome IN ('active','logout','recovered')),
 recovered_at timestamptz,
 PRIMARY KEY(world_id,char_id),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE"("bWorldID","dwCharID") ON DELETE CASCADE,
 CHECK((outcome='recovered')=(recovered_at IS NOT NULL))
);
COMMENT ON TABLE app_world.map_checkpoints IS
 'Latest atomic core receipt, not an event journal. Contract 1 authorizes recovery to this committed core only; unsaved transient changes after saved_at are lost on a crash. Receipt identity and current core must match. Older ready/orphaned claims without a receipt remain blocked. Inventory, rewards, active buffs and secondary transfers require their own durable contracts before admission.';
REVOKE ALL ON app_world.map_checkpoints FROM PUBLIC;
