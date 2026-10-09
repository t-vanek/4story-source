-- Operational primary transfer. Historical schemas and migrations 001–019 stay immutable.
-- A connection can become primary repeatedly; its authority epoch prevents ABA
-- writes from an earlier primary tenure on the same process/socket generation.
ALTER TABLE app_world.map_sessions ADD COLUMN authority_epoch bigint NOT NULL DEFAULT 0 CHECK(authority_epoch>=0);
ALTER TABLE app_world.map_checkpoints ADD COLUMN authority_epoch bigint NOT NULL DEFAULT 0 CHECK(authority_epoch>=0);
ALTER TABLE app_world.map_checkpoints DROP CONSTRAINT map_checkpoints_recovery_contract_check;
ALTER TABLE app_world.map_checkpoints ADD CONSTRAINT map_checkpoints_recovery_contract_check CHECK(recovery_contract IN (1,2));
ALTER TABLE app_world.map_checkpoints
 ADD COLUMN transfer_body bytea,
 ADD COLUMN transfer_hash text,
 ADD COLUMN character_manifest text,
 ADD COLUMN routing_manifest text,
 ADD COLUMN actor_manifest text,
 ADD CONSTRAINT map_checkpoint_transfer_graph CHECK(
  (recovery_contract=1 AND transfer_body IS NULL AND transfer_hash IS NULL AND character_manifest IS NULL AND routing_manifest IS NULL AND actor_manifest IS NULL)
  OR (recovery_contract=2 AND transfer_body IS NOT NULL AND transfer_hash IS NOT NULL AND character_manifest IS NOT NULL AND routing_manifest IS NOT NULL AND actor_manifest IS NOT NULL
      AND octet_length(transfer_body) BETWEEN 10 AND 65527 AND transfer_hash=encode(sha256(transfer_body),'hex')
      AND character_manifest ~ '^[0-9a-f]{64}$' AND routing_manifest ~ '^[0-9a-f]{64}$' AND actor_manifest ~ '^[0-9a-f]{64}$'));
ALTER TABLE app_world.map_replicas ADD COLUMN primary_epoch bigint NOT NULL DEFAULT 0 CHECK(primary_epoch>=0);
ALTER TABLE app_world.map_sessions DROP CONSTRAINT map_sessions_phase_check;
ALTER TABLE app_world.map_sessions ADD CONSTRAINT map_sessions_phase_check
 CHECK(phase IN ('claimed','loaded','ready','transferring','quarantined','orphaned'));

CREATE TABLE app_world.map_transfers (
 transfer_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 user_id integer NOT NULL,
 session_key integer NOT NULL,
 channel smallint NOT NULL CHECK(channel BETWEEN 0 AND 255),
 source_server smallint NOT NULL CHECK(source_server BETWEEN 1 AND 255),
 source_token text NOT NULL CHECK(source_token ~ '^[0-9a-f]{64}$'),
 source_connection bigint NOT NULL CHECK(source_connection>0),
 source_ip bigint NOT NULL CHECK(source_ip BETWEEN 1 AND 4294967295),
 source_port integer NOT NULL CHECK(source_port BETWEEN 1 AND 65535),
 source_epoch bigint NOT NULL CHECK(source_epoch>=0 AND source_epoch<9223372036854775807),
 target_server smallint NOT NULL CHECK(target_server BETWEEN 1 AND 255),
 target_token text NOT NULL CHECK(target_token ~ '^[0-9a-f]{64}$'),
 target_connection bigint NOT NULL CHECK(target_connection>0),
 target_epoch bigint NOT NULL CHECK(target_epoch>0),
 routing_manifest text NOT NULL CHECK(routing_manifest ~ '^[0-9a-f]{64}$'),
 character_manifest text NOT NULL CHECK(character_manifest ~ '^[0-9a-f]{64}$'),
 actor_manifest text NOT NULL CHECK(actor_manifest ~ '^[0-9a-f]{64}$'),
 body bytea NOT NULL CHECK(octet_length(body) BETWEEN 10 AND 65527),
 body_sha256 text NOT NULL CHECK(body_sha256 ~ '^[0-9a-f]{64}$'),
 core_fingerprint text NOT NULL CHECK(core_fingerprint ~ '^[0-9a-f]{64}$'),
 phase text NOT NULL CHECK(phase IN ('prepared','consumed','cancelled')),
 prepared_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 expires_at timestamptz NOT NULL DEFAULT clock_timestamp()+interval '5 seconds',
 settled_at timestamptz,
 CHECK(source_server<>target_server),
 CHECK(body_sha256=encode(sha256(body),'hex')),
 CHECK(target_epoch=source_epoch+1),
 CHECK((phase='prepared')=(settled_at IS NULL)),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE"("bWorldID","dwCharID") ON DELETE CASCADE
);
CREATE UNIQUE INDEX map_transfers_one_prepared ON app_world.map_transfers(world_id,char_id) WHERE phase='prepared';
CREATE INDEX map_transfers_target_receipt ON app_world.map_transfers(world_id,char_id,target_token,target_connection,target_epoch);
COMMENT ON TABLE app_world.map_transfers IS
 'Exact original transfer body plus source/target process, connection and authority identities. Prepared state fences core writes. Consume rebinds primary/checkpoint/replicas atomically. Receipts survive claim deletion for lost-response confirmation and diagnosis. Bodies may contain private character/security data; no PUBLIC access or logging. A prepared transfer is not permission to recover by silently dropping its transient graph.';
REVOKE ALL ON app_world.map_transfers FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.map_transfers_transfer_id_seq FROM PUBLIC;
