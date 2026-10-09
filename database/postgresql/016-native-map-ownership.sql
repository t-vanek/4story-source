-- Operational Map ownership only. No historical records or source changes.
CREATE TABLE app_world.map_runtime_owner (
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    server_id smallint NOT NULL CHECK(server_id BETWEEN 0 AND 255),
    owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
    backend_pid integer NOT NULL,
    acquired_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(world_id,server_id)
);
CREATE TABLE app_world.map_sessions (
    world_id smallint NOT NULL,
    char_id integer NOT NULL,
    user_id integer NOT NULL REFERENCES app_global."TACCOUNT_PW"("dwUserID"),
    session_key integer NOT NULL UNIQUE REFERENCES app_global."TCURRENTUSER"("dwKEY"),
    channel smallint NOT NULL CHECK(channel BETWEEN 0 AND 255),
    server_id smallint NOT NULL CHECK(server_id BETWEEN 0 AND 255),
    owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
    connection_id bigint NOT NULL CHECK(connection_id>0),
    routing_manifest text NOT NULL CHECK(routing_manifest ~ '^[0-9a-f]{64}$'),
    character_manifest text NOT NULL CHECK(character_manifest ~ '^[0-9a-f]{64}$'),
    phase text NOT NULL CHECK(phase IN ('claimed','loaded','ready','quarantined','orphaned')),
    claimed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(world_id,char_id),
    UNIQUE(world_id,server_id,owner_token,connection_id),
    FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE"("bWorldID","dwCharID"),
    FOREIGN KEY(world_id,server_id) REFERENCES app_world.map_runtime_owner(world_id,server_id)
);
COMMENT ON TABLE app_world.map_sessions IS
 'Consumed Login handoffs. The current-user FK deliberately restricts deletion: Login cannot erase a Map claim. Every Map read/write checks process token and connection generation. Lost-process ready/quarantined state is retained as orphaned until durable recovery; safe pre-gameplay claims may be closed by the replacement owner.';
REVOKE ALL ON app_world.map_runtime_owner,app_world.map_sessions FROM PUBLIC;
