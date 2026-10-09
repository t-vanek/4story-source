-- Fresh-primary recovery contract v3: core and learned-skill cooldowns commit
-- together. v1 core-only and v2 complete transfer graph receipts remain readable.
-- Historical schemas, backups and migrations 001-020 remain unchanged.
ALTER TABLE app_world.map_checkpoints ADD COLUMN skill_state jsonb;
ALTER TABLE app_world.map_checkpoints DROP CONSTRAINT map_checkpoints_recovery_contract_check;
ALTER TABLE app_world.map_checkpoints ADD CONSTRAINT map_checkpoints_recovery_contract_check CHECK(recovery_contract IN (1,2,3));
ALTER TABLE app_world.map_checkpoints DROP CONSTRAINT map_checkpoint_transfer_graph;
ALTER TABLE app_world.map_checkpoints
ADD CONSTRAINT map_checkpoint_transfer_graph CHECK(
  (recovery_contract IN (1,3) AND transfer_body IS NULL AND transfer_hash IS NULL AND character_manifest IS NULL AND routing_manifest IS NULL AND actor_manifest IS NULL)
  OR (recovery_contract=2 AND transfer_body IS NOT NULL AND transfer_hash IS NOT NULL AND character_manifest IS NOT NULL AND routing_manifest IS NOT NULL AND actor_manifest IS NOT NULL
      AND octet_length(transfer_body) BETWEEN 10 AND 65527 AND transfer_hash=encode(sha256(transfer_body),'hex')
      AND character_manifest ~ '^[0-9a-f]{64}$' AND routing_manifest ~ '^[0-9a-f]{64}$' AND actor_manifest ~ '^[0-9a-f]{64}$'));

ALTER TABLE app_world.map_checkpoints ADD CONSTRAINT map_checkpoint_skills CHECK(
 (recovery_contract IN (1,2) AND skill_state IS NULL) OR
 (recovery_contract=3 AND skill_state IS NOT NULL AND jsonb_typeof(skill_state)='array' AND jsonb_array_length(skill_state)<=255));
CREATE FUNCTION app_world.map_skill_state(p_world smallint,p_char integer)
RETURNS jsonb LANGUAGE sql STABLE AS $$
 SELECT COALESCE(jsonb_agg(jsonb_build_array("wSkillID"::integer & 65535,"bLevel","dwRemainTick"::bigint & 4294967295)
  ORDER BY ("wSkillID"::integer & 65535)), '[]'::jsonb)
 FROM app_world."TSKILLTABLE" WHERE "bWorldID"=p_world AND "dwCharID"=p_char
$$;
CREATE FUNCTION app_world.map_checkpoint_matches(p app_world.map_checkpoints)
RETURNS boolean LANGUAGE sql STABLE AS $$
 SELECT COALESCE(p.core_state=app_world.map_core_state(p.world_id,p.char_id)
  AND p.recovery_contract IN (1,2,3)
  AND (p.recovery_contract<>3 OR p.skill_state=app_world.map_skill_state(p.world_id,p.char_id)),false)
$$;
REVOKE ALL ON FUNCTION app_world.map_skill_state(smallint,integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_checkpoint_matches(app_world.map_checkpoints) FROM PUBLIC;
COMMENT ON COLUMN app_world.map_checkpoints.skill_state IS
 'Contract 3 exact unsigned [skill ID, learned rank, remaining milliseconds] rows, sorted by unsigned ID. Compared with mutable TSKILLTABLE before checkpoint, logout, transfer and crash recovery. No offline wall-clock decrement. Full graph contract 2 remains authoritative after transfer. Skill learning/rank changes require a separate domain operation.';
