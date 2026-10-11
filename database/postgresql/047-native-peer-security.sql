-- New operational state for the existing modern PeerSecurityGate.
-- These tables are not reconstructed historical TGAME/TGLOBAL content.
CREATE TABLE runtime_control.peer_trust (
    group_id smallint NOT NULL CHECK (group_id BETWEEN 0 AND 255),
    server_id smallint NOT NULL CHECK (server_id BETWEEN 0 AND 255),
    type_id smallint NOT NULL CHECK (type_id BETWEEN 0 AND 255),
    peer_name text NOT NULL,
    secret_hex text NOT NULL CHECK (secret_hex ~ '^([0-9a-fA-F]{2}){32,}$'),
    ip_allowlist text NOT NULL,
    enabled boolean NOT NULL DEFAULT true,
    fail_count bigint NOT NULL DEFAULT 0 CHECK (fail_count BETWEEN 0 AND 4294967295),
    ok_count bigint NOT NULL DEFAULT 0 CHECK (ok_count BETWEEN 0 AND 4294967295),
    last_seen timestamptz,
    PRIMARY KEY (group_id, server_id)
);
CREATE TABLE runtime_control.peer_auth_log (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    group_id smallint NOT NULL CHECK (group_id BETWEEN 0 AND 255),
    server_id smallint NOT NULL CHECK (server_id BETWEEN 0 AND 255),
    type_id smallint NOT NULL CHECK (type_id BETWEEN 0 AND 255),
    remote_ip text NOT NULL,
    outcome smallint NOT NULL CHECK (outcome BETWEEN 0 AND 7),
    reason text NOT NULL DEFAULT '',
    recorded_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE INDEX peer_auth_log_retention ON runtime_control.peer_auth_log(recorded_at);
COMMENT ON TABLE runtime_control.peer_auth_log IS
    'Modern peer authentication audit; unknown/removed identities deliberately have no foreign key to the current trust store.';
REVOKE ALL ON runtime_control.peer_trust,runtime_control.peer_auth_log FROM PUBLIC;
REVOKE ALL ON SEQUENCE runtime_control.peer_auth_log_id_seq FROM PUBLIC;
