-- Additive native FINISHSKILL receipts. No original protocol or source table change.
CREATE TABLE app_world.skill_finish_operations (
 finish_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 accepted_cast_id bigint NOT NULL UNIQUE REFERENCES app_world.accepted_skill_casts ON DELETE CASCADE,
 previous_finish_id bigint NOT NULL CHECK(previous_finish_id>=0),
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 session_key bigint NOT NULL CHECK(session_key BETWEEN 0 AND 4294967295),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 request bytea NOT NULL CHECK(octet_length(request) BETWEEN 34 AND 1309),
 responses jsonb NOT NULL CHECK(jsonb_typeof(responses)='array'),
 before_hp bigint NOT NULL CHECK(before_hp BETWEEN 0 AND 4294967295),
 after_hp bigint NOT NULL CHECK(after_hp BETWEEN 0 AND 4294967295),
 before_mp bigint NOT NULL CHECK(before_mp BETWEEN 0 AND 4294967295),
 after_mp bigint NOT NULL CHECK(after_mp BETWEEN 0 AND 4294967295),
 core_fingerprint text NOT NULL CHECK(core_fingerprint ~ '^[0-9a-f]{64}$'),
 committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK(finish_id>previous_finish_id),
 UNIQUE(world_id,char_id,previous_finish_id),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE INDEX skill_finish_character_latest ON app_world.skill_finish_operations(world_id,char_id,finish_id DESC);
REVOKE ALL ON app_world.skill_finish_operations FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.skill_finish_operations_finish_id_seq FROM PUBLIC;
COMMENT ON TABLE app_world.skill_finish_operations IS
 'One atomic finish per accepted cast, with exact input and ordered output. Supports bounded instant self-cures; unsupported combat stays explicit. FIFO matching in the same login session, without invented expiry or client cast ID, cannot distinguish a replay from an identical later legitimate outstanding finish. No automatic retry after uncertain commit.';
