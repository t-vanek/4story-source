-- New operational state for the already-existing modern discovery protocol.
-- These tables are not claimed to exist in either original backup.
CREATE SEQUENCE runtime_control.peer_epoch AS bigint NO CYCLE;
CREATE TABLE runtime_control.peer_registry (
    service_id integer PRIMARY KEY CHECK(service_id BETWEEN 0 AND 16777215),
    name_bytes bytea NOT NULL, reported_addr text NOT NULL,
    reported_port integer NOT NULL CHECK(reported_port BETWEEN 0 AND 65535),
    version_bytes bytea NOT NULL, pid bigint NOT NULL CHECK(pid BETWEEN 0 AND 4294967295),
    start_unix bigint NOT NULL,
    cur_users bigint NOT NULL CHECK(cur_users BETWEEN 0 AND 4294967295),
    max_users bigint NOT NULL CHECK(max_users BETWEEN 0 AND 4294967295),
    lease_epoch bigint NOT NULL UNIQUE CHECK(lease_epoch>0),
    registered_at timestamptz NOT NULL, heartbeat_at timestamptz NOT NULL
);
CREATE TABLE runtime_control.peer_status_log (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    group_id smallint NOT NULL CHECK(group_id BETWEEN 0 AND 255),
    server_id smallint NOT NULL CHECK(server_id BETWEEN 0 AND 255),
    type_id smallint NOT NULL CHECK(type_id BETWEEN 0 AND 255),
    old_status smallint NOT NULL, new_status smallint NOT NULL,
    reason_bytes bytea NOT NULL, changed_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE INDEX control_peer_status_time ON runtime_control.peer_status_log(changed_at);
CREATE TABLE runtime_control.peer_metrics (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    group_id smallint NOT NULL CHECK(group_id BETWEEN 0 AND 255),
    server_id smallint NOT NULL CHECK(server_id BETWEEN 0 AND 255),
    type_id smallint NOT NULL CHECK(type_id BETWEEN 0 AND 255),
    sessions bigint NOT NULL CHECK(sessions BETWEEN 0 AND 4294967295),
    users bigint NOT NULL CHECK(users BETWEEN 0 AND 4294967295),
    active_users bigint NOT NULL CHECK(active_users BETWEEN 0 AND 4294967295),
    sampled_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE INDEX control_peer_metrics_time ON runtime_control.peer_metrics(sampled_at);
REVOKE ALL ON runtime_control.peer_registry,runtime_control.peer_status_log,runtime_control.peer_metrics FROM PUBLIC;
REVOKE ALL ON SEQUENCE runtime_control.peer_epoch,runtime_control.peer_status_log_id_seq,runtime_control.peer_metrics_id_seq FROM PUBLIC;
