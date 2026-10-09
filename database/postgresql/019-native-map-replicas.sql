-- Operational secondary Map admission. Historical catalogs/rows are unchanged.
-- A replica never owns mutable character state or the current-user reservation.
CREATE TABLE app_world.map_replicas (
    world_id smallint NOT NULL,
    char_id integer NOT NULL,
    primary_server smallint NOT NULL CHECK(primary_server BETWEEN 1 AND 255),
    primary_token text NOT NULL CHECK(primary_token ~ '^[0-9a-f]{64}$'),
    primary_connection bigint NOT NULL CHECK(primary_connection>0),
    target_server smallint NOT NULL CHECK(target_server BETWEEN 1 AND 255),
    target_token text NOT NULL CHECK(target_token ~ '^[0-9a-f]{64}$'),
    endpoint_ip bigint NOT NULL CHECK(endpoint_ip BETWEEN 1 AND 4294967295),
    endpoint_port integer NOT NULL CHECK(endpoint_port BETWEEN 1 AND 65535),
    map_id integer NOT NULL CHECK(map_id BETWEEN 0 AND 65535),
    position_x real NOT NULL CHECK(position_x>=0 AND position_x<65536),
    position_z real NOT NULL CHECK(position_z>=0 AND position_z<65536),
    routing_manifest text NOT NULL CHECK(routing_manifest ~ '^[0-9a-f]{64}$'),
    character_manifest text NOT NULL CHECK(character_manifest ~ '^[0-9a-f]{64}$'),
    actor_manifest text NOT NULL CHECK(actor_manifest ~ '^[0-9a-f]{64}$'),
    phase text NOT NULL CHECK(phase IN ('granted','claimed','loaded','ready','retired')),
    connection_id bigint CHECK(connection_id>0),
    granted_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    expires_at timestamptz NOT NULL DEFAULT clock_timestamp()+interval '60 seconds',
    updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(world_id,char_id,target_server),
    UNIQUE(world_id,target_server,target_token,connection_id),
    FOREIGN KEY(world_id,char_id) REFERENCES app_world.map_sessions(world_id,char_id) ON DELETE CASCADE,
    FOREIGN KEY(world_id,target_server) REFERENCES app_world.map_runtime_owner(world_id,server_id),
    CHECK(primary_server<>target_server),
    CHECK((phase='granted')=(connection_id IS NULL))
);
COMMENT ON TABLE app_world.map_replicas IS
 'One-use secondary endpoint grants bound to exact primary and target process generations. Only source CHARDATA/ENTERCHAR supplies transient state; replicas never load or save the primary graph. A retired row prevents replay until an explicit new route decision. Primary close/recovery cascades all grants and replicas. Target restart deletes only its old-token rows. Grant expiry does not expire a live accepted replica.';
REVOKE ALL ON app_world.map_replicas FROM PUBLIC;
