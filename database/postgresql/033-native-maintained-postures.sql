-- Fresh-primary v4 adds the ordered eight-field maintained-skill contract.
-- All backups and migrations 001-032 remain unchanged. Full graph v2 remains
-- authoritative after transfer; its stale normalized children are never written.
CREATE TABLE app_world.map_maintained_effects (
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 ordinal smallint NOT NULL CHECK(ordinal BETWEEN 0 AND 254),
 skill_id integer NOT NULL CHECK(skill_id BETWEEN 1 AND 65535),
 skill_level smallint NOT NULL CHECK(skill_level BETWEEN 1 AND 255),
 remaining bigint NOT NULL CHECK(remaining BETWEEN 0 AND 4294967295),
 attack_type smallint NOT NULL CHECK(attack_type BETWEEN 0 AND 255),
 attack_id bigint NOT NULL CHECK(attack_id BETWEEN 0 AND 4294967295),
 host_type smallint NOT NULL CHECK(host_type BETWEEN 0 AND 255),
 host_id bigint NOT NULL CHECK(host_id BETWEEN 0 AND 4294967295),
 attack_country smallint NOT NULL CHECK(attack_country BETWEEN 0 AND 255),
 PRIMARY KEY(world_id,char_id,ordinal),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE FUNCTION app_world.map_maintain_state(p_world smallint,p_char integer)
RETURNS jsonb LANGUAGE sql STABLE AS $$
 SELECT COALESCE(jsonb_agg(jsonb_build_array(skill_id,skill_level,remaining,attack_type,attack_id,host_type,host_id,attack_country)
  ORDER BY ordinal), '[]'::jsonb)
 FROM app_world.map_maintained_effects WHERE world_id=p_world AND char_id=p_char
$$;
ALTER TABLE app_world.map_checkpoints ADD COLUMN maintain_state jsonb;
ALTER TABLE app_world.map_checkpoints DROP CONSTRAINT map_checkpoints_recovery_contract_check;
ALTER TABLE app_world.map_checkpoints ADD CONSTRAINT map_checkpoints_recovery_contract_check CHECK(recovery_contract IN (1,2,3,4));
ALTER TABLE app_world.map_checkpoints DROP CONSTRAINT map_checkpoint_transfer_graph;
ALTER TABLE app_world.map_checkpoints ADD CONSTRAINT map_checkpoint_transfer_graph CHECK(
 (recovery_contract IN (1,3,4) AND transfer_body IS NULL AND transfer_hash IS NULL AND character_manifest IS NULL AND routing_manifest IS NULL AND actor_manifest IS NULL)
 OR (recovery_contract=2 AND transfer_body IS NOT NULL AND transfer_hash IS NOT NULL AND character_manifest IS NOT NULL AND routing_manifest IS NOT NULL AND actor_manifest IS NOT NULL
   AND octet_length(transfer_body) BETWEEN 10 AND 65527 AND transfer_hash=encode(sha256(transfer_body),'hex')
   AND character_manifest ~ '^[0-9a-f]{64}$' AND routing_manifest ~ '^[0-9a-f]{64}$' AND actor_manifest ~ '^[0-9a-f]{64}$'));
ALTER TABLE app_world.map_checkpoints DROP CONSTRAINT map_checkpoint_skills;
ALTER TABLE app_world.map_checkpoints ADD CONSTRAINT map_checkpoint_skills CHECK(
 (recovery_contract IN (1,2) AND skill_state IS NULL) OR
 (recovery_contract IN (3,4) AND skill_state IS NOT NULL AND jsonb_typeof(skill_state)='array' AND jsonb_array_length(skill_state)<=255));
ALTER TABLE app_world.map_checkpoints ADD CONSTRAINT map_checkpoint_maintains CHECK(
 (recovery_contract IN (1,2,3) AND maintain_state IS NULL) OR
 (recovery_contract=4 AND maintain_state IS NOT NULL AND jsonb_typeof(maintain_state)='array' AND jsonb_array_length(maintain_state) BETWEEN 1 AND 255));
CREATE OR REPLACE FUNCTION app_world.map_checkpoint_matches(p app_world.map_checkpoints)
RETURNS boolean LANGUAGE sql STABLE AS $$
 SELECT COALESCE(p.core_state=app_world.map_core_state(p.world_id,p.char_id)
  AND p.recovery_contract IN (1,2,3,4)
  AND (p.recovery_contract NOT IN (3,4) OR p.skill_state=app_world.map_skill_state(p.world_id,p.char_id))
  AND (p.recovery_contract=2 OR
       COALESCE(p.maintain_state,'[]'::jsonb)=app_world.map_maintain_state(p.world_id,p.char_id)),false)
$$;
-- The existing item ledger's state_contract identifies storage (2 graph / 3
-- normalized), not recovery schema. Record effects explicitly with each new
-- equipment receipt; historical receipts stay NULL and are never rewritten.
ALTER TABLE app_world.equipment_operations ADD COLUMN before_effects jsonb;
ALTER TABLE app_world.equipment_operations ADD COLUMN after_effects jsonb;
ALTER TABLE app_world.equipment_operations ADD CONSTRAINT equipment_effects CHECK(
 (before_effects IS NULL AND after_effects IS NULL) OR
 (before_effects IS NOT NULL AND after_effects IS NOT NULL AND jsonb_typeof(before_effects)='array' AND jsonb_typeof(after_effects)='array'
  AND jsonb_array_length(before_effects)<=255 AND jsonb_array_length(after_effects)<=255));
REVOKE ALL ON app_world.map_maintained_effects FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_maintain_state(smallint,integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_checkpoint_matches(app_world.map_checkpoints) FROM PUBLIC;
COMMENT ON TABLE app_world.map_maintained_effects IS
 'Original maintained-skill fields with stable vector order. Native runtime currently admits permanent warrior postures 131/132. Zero remaining is permanent for zero-duration templates; it is not an expired timer. Ordinary saves compare this state and cannot create/remove effects. Full transfer graphs remain authoritative.';
