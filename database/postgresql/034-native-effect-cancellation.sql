-- Original owned-PC CS_SKILLEND_REQ. Keep all prior migrations and receipts.
CREATE TABLE app_world.maintained_effect_operations (
 operation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 state_contract smallint NOT NULL CHECK(state_contract IN (2,3)),
 request bytea NOT NULL CHECK(octet_length(request)=19),
 removed smallint NOT NULL CHECK(removed IN (0,1)),
 before_effects jsonb NOT NULL CHECK(jsonb_typeof(before_effects)='array' AND jsonb_array_length(before_effects)<=255),
 after_effects jsonb NOT NULL CHECK(jsonb_typeof(after_effects)='array' AND jsonb_array_length(after_effects)<=255),
 before_graph_hash text CHECK(before_graph_hash ~ '^[0-9a-f]{64}$'),
 after_graph_hash text CHECK(after_graph_hash ~ '^[0-9a-f]{64}$'),
 core_fingerprint text NOT NULL CHECK(core_fingerprint ~ '^[0-9a-f]{64}$'),
 committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK(jsonb_array_length(before_effects)-jsonb_array_length(after_effects)=removed),
 CHECK((state_contract=2 AND before_graph_hash IS NOT NULL AND after_graph_hash IS NOT NULL)
    OR (state_contract=3 AND before_graph_hash IS NULL AND after_graph_hash IS NULL)),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
REVOKE ALL ON app_world.maintained_effect_operations FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.maintained_effect_operations_operation_id_seq FROM PUBLIC;
COMMENT ON TABLE app_world.maintained_effect_operations IS
 'Exact original 19-byte own-PC SKILLEND request, owner/generation/epoch, original ordered maintained collections and graph/core fingerprints. First matching attacker/type/skill is removed; a legitimate absent match records removed=0 and receives the original ACK only. Effects, derived stats/core, sampled skill timers and recovery receipt commit together. No automatic retry after unknown commit. Replica ACKs never write this table.';
