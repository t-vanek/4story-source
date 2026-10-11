-- Source TGLOBAL_RAGEZONE.TSECURECODE has zero rows in the pinned backup.
-- Original wire bytes remain unchanged. This account state is never populated
-- implicitly from untrusted/stale transfer graphs or historical nullable rows.
CREATE TABLE app_global.secure_codes (
 user_id integer PRIMARY KEY REFERENCES app_global."TACCOUNT_PW"("dwUserID"),
 code bytea NOT NULL CHECK(octet_length(code)<=55 AND position(decode('00','hex') in code)=0),
 disabled boolean NOT NULL,
 tries smallint NOT NULL CHECK(tries BETWEEN 0 AND 10),
 deadline_ms bigint NOT NULL CHECK(deadline_ms>=0),
 source_tick bigint NOT NULL CHECK(source_tick BETWEEN 0 AND 4294967295),
 unlocked_key bigint NOT NULL CHECK(unlocked_key BETWEEN 0 AND 4294967295),
 revision bigint NOT NULL CHECK(revision>0),
 CHECK(tries<10 OR deadline_ms>0)
);
CREATE TABLE app_world.secure_code_operations (
 operation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 user_id integer NOT NULL REFERENCES app_global."TACCOUNT_PW"("dwUserID"),
 previous_revision bigint NOT NULL CHECK(previous_revision>=0),
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 session_key bigint NOT NULL,
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 request_opcode integer NOT NULL CHECK(request_opcode IN (21888,21890,21892,21894,21896,21904,21906)),
 response_opcode integer NOT NULL,
 response bytea NOT NULL CHECK(octet_length(response) BETWEEN 1 AND 2),
 committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 UNIQUE(user_id,previous_revision),
 CHECK(operation_id>previous_revision),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE"
);
REVOKE ALL ON app_global.secure_codes,app_world.secure_code_operations FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.secure_code_operations_operation_id_seq FROM PUBLIC;
COMMENT ON TABLE app_global.secure_codes IS 'Original case-sensitive raw credential bytes; restricted Map storage. Deadline survives Map restart and DWORD tick wrap. Unlock is scoped to the non-recycled Login key. Disable/create initialization and omitted transfer disabled flag are repaired through the account authority.';
COMMENT ON TABLE app_world.secure_code_operations IS 'Append-only mutation receipts without request codes. Account row lock, primary epoch and revision fence every mutation. Commit before response, never retry unknown outcomes. No synthetic success on database failure.';
