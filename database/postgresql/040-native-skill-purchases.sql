-- Atomic original SKILLBUY operations. Old characters/receipts remain intact.
CREATE TABLE app_world.skill_purchase_operations (
 purchase_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 previous_purchase_id bigint NOT NULL CHECK(previous_purchase_id>=0),
 session_key bigint NOT NULL CHECK(session_key BETWEEN 0 AND 4294967295),
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 trainer_manifest text NOT NULL CHECK(trainer_manifest ~ '^[0-9a-f]{64}$'),
 npc_id integer NOT NULL CHECK(npc_id BETWEEN 0 AND 65535),
 skill_id integer NOT NULL CHECK(skill_id BETWEEN 0 AND 65535),
 result smallint NOT NULL CHECK(result BETWEEN 0 AND 255),
 learned_rank smallint NOT NULL CHECK(learned_rank BETWEEN 0 AND 255),
 spent bigint NOT NULL CHECK(spent BETWEEN 0 AND 4294967295),
 request bytea NOT NULL CHECK(octet_length(request)=4),
 acknowledgement bytea NOT NULL CHECK(octet_length(acknowledgement)=30),
 title_packets jsonb NOT NULL CHECK(jsonb_typeof(title_packets)='array'),
 before_titles jsonb NOT NULL CHECK(jsonb_typeof(before_titles)='array'),
 after_titles jsonb NOT NULL CHECK(jsonb_typeof(after_titles)='array'),
 purchased_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK(purchase_id>previous_purchase_id),
 CHECK(result=0 OR spent=0),
 UNIQUE(world_id,char_id,previous_purchase_id),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE INDEX skill_purchase_character_latest ON app_world.skill_purchase_operations(world_id,char_id,purchase_id DESC);
REVOKE ALL ON app_world.skill_purchase_operations FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.skill_purchase_operations_purchase_id_seq FROM PUBLIC;
COMMENT ON TABLE app_world.skill_purchase_operations IS
 'Money, points, learned rank, source gold-title side effects, checkpoint and this receipt commit together. A fenced snapshot head prevents stale saves and concurrent purchases from overwriting accepted changes. The original request has no operation nonce: a later explicit repeat is another rank request; unknown commit outcomes close the session and are never retried automatically.';
