-- Accepted original-protocol casts. This is a modern atomic recovery contract;
-- original .bak tables, earlier migrations and existing receipts are unchanged.
CREATE TABLE app_world.accepted_skill_casts (
 cast_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 session_key bigint NOT NULL CHECK(session_key BETWEEN 0 AND 4294967295),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 previous_cast_id bigint NOT NULL CHECK(previous_cast_id>=0),
 state_contract smallint NOT NULL CHECK(state_contract IN (2,3)),
 skill_id integer NOT NULL CHECK(skill_id BETWEEN 1 AND 65535),
 skill_rank smallint NOT NULL CHECK(skill_rank BETWEEN 1 AND 255),
 is_loop boolean NOT NULL,
 request bytea NOT NULL,
 acknowledgement bytea NOT NULL,
 before_hp bigint NOT NULL CHECK(before_hp BETWEEN 0 AND 4294967295),
 after_hp bigint NOT NULL CHECK(after_hp BETWEEN 0 AND 4294967295),
 before_mp bigint NOT NULL CHECK(before_mp BETWEEN 0 AND 4294967295),
 after_mp bigint NOT NULL CHECK(after_mp BETWEEN 0 AND 4294967295),
 before_skills jsonb NOT NULL CHECK(jsonb_typeof(before_skills)='array'),
 after_skills jsonb NOT NULL CHECK(jsonb_typeof(after_skills)='array'),
 before_effects jsonb NOT NULL CHECK(jsonb_typeof(before_effects)='array'),
 after_effects jsonb NOT NULL CHECK(jsonb_typeof(after_effects)='array'),
 before_graph_hash text CHECK(before_graph_hash ~ '^[0-9a-f]{64}$'),
 after_graph_hash text CHECK(after_graph_hash ~ '^[0-9a-f]{64}$'),
 core_fingerprint text NOT NULL CHECK(core_fingerprint ~ '^[0-9a-f]{64}$'),
 committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK(cast_id>previous_cast_id),
 CHECK(octet_length(request) BETWEEN CASE WHEN is_loop THEN 23 ELSE 32 END AND 1562),
 CHECK(octet_length(acknowledgement) BETWEEN CASE WHEN is_loop THEN 45 ELSE 62 END AND 142),
 CHECK((state_contract=2 AND before_graph_hash IS NOT NULL AND after_graph_hash IS NOT NULL)
    OR (state_contract=3 AND before_graph_hash IS NULL AND after_graph_hash IS NULL)),
 UNIQUE(world_id,char_id,previous_cast_id),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE INDEX accepted_skill_casts_character_latest ON app_world.accepted_skill_casts(world_id,char_id,cast_id DESC);
ALTER TABLE app_world.skill_item_consumptions ADD COLUMN accepted_cast_id bigint REFERENCES app_world.accepted_skill_casts(cast_id) ON DELETE CASCADE;
REVOKE ALL ON app_world.accepted_skill_casts FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.accepted_skill_casts_cast_id_seq FROM PUBLIC;
COMMENT ON TABLE app_world.accepted_skill_casts IS
 'Append-only accepted SKILLUSE/LOOPSKILL request and source ACK with ordered expanded targets. Costs, timers, supported effect removals, all item receipts and recovery checkpoint commit together. previous_cast_id fences stale live snapshots even for free casts. No request-body deduplication: distinct legal zero-cooldown requests remain distinct casts. No automatic retry of unknown commits. This records acceptance; later authoritative target-hit consumption must enforce its own one-use occurrence contract.';
