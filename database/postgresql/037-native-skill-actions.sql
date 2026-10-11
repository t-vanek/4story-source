-- Native ACTION validation and charge provenance; no client/SS wire changes.
CREATE TABLE app_world.skill_action_operations (
 action_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 previous_action_id bigint NOT NULL CHECK(previous_action_id>=0),
 session_key bigint NOT NULL CHECK(session_key BETWEEN 0 AND 4294967295),
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 channel_id smallint NOT NULL CHECK(channel_id BETWEEN 0 AND 255),
 map_id integer NOT NULL CHECK(map_id BETWEEN 0 AND 65535),
 skill_id integer NOT NULL CHECK(skill_id BETWEEN 0 AND 65535),
 result smallint NOT NULL CHECK(result BETWEEN 0 AND 255),
 charge_ms bigint NOT NULL CHECK(charge_ms BETWEEN 0 AND 4294967295),
 request bytea NOT NULL CHECK(octet_length(request)=19),
 acknowledgement bytea NOT NULL CHECK(octet_length(acknowledgement)=17),
 started_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK(action_id>previous_action_id),
 CHECK(charge_ms=0 OR (skill_id<>0 AND result=0)),
 UNIQUE(world_id,char_id,previous_action_id),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE INDEX skill_action_character_latest ON app_world.skill_action_operations(world_id,char_id,session_key,action_id DESC);
ALTER TABLE app_world.accepted_skill_casts ADD COLUMN charge_action_id bigint UNIQUE
 REFERENCES app_world.skill_action_operations(action_id);
REVOKE ALL ON app_world.skill_action_operations FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.skill_action_operations_action_id_seq FROM PUBLIC;
COMMENT ON TABLE app_world.skill_action_operations IS
 'Original ACTION input/result and charge origin. Latest successful action replaces/cancels an unconsumed charge; a cast binds it once. Elapsed charge uses the shared database clock across Map owners, not a client timestamp. New login keys cannot inherit old charges. Existing casts retain NULL charge provenance.';
