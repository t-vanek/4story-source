-- Operational world mapping and expiring Login-to-Map reservation. No backup rows seeded.
CREATE TABLE app_world.routing_worlds (
    group_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    source_group smallint NOT NULL CHECK(source_group BETWEEN 0 AND 255)
);
COMMENT ON TABLE app_world.routing_worlds IS
 'Explicit operator mapping from mutable application world to the group in its pinned backup routing charts; these are distinct identities.';
CREATE TABLE app_global.map_handoff (
    session_key integer PRIMARY KEY REFERENCES app_global."TCURRENTUSER"("dwKEY") ON DELETE CASCADE,
    user_id integer NOT NULL REFERENCES app_global."TACCOUNT_PW"("dwUserID"),
    world_id smallint NOT NULL,
    char_id integer NOT NULL,
    channel smallint NOT NULL CHECK(channel BETWEEN 0 AND 255),
    server_id smallint NOT NULL CHECK(server_id BETWEEN 0 AND 255),
    server_ip inet NOT NULL CHECK(family(server_ip)=4),
    server_port integer NOT NULL CHECK(server_port BETWEEN 1 AND 65535),
    routing_manifest text NOT NULL CHECK(routing_manifest ~ '^[0-9a-f]{64}$'),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    expires_at timestamptz NOT NULL DEFAULT clock_timestamp()+interval '60 seconds',
    UNIQUE(world_id,char_id),
    FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE"("bWorldID","dwCharID")
);
CREATE INDEX map_handoff_expiry ON app_global.map_handoff(expires_at);
REVOKE ALL ON app_global.map_handoff,app_world.routing_worlds FROM PUBLIC;
COMMENT ON TABLE app_global.map_handoff IS
 'One pending native START per session/character, committed with endpoint selection and TCURRENTUSER claims. Future native Map claim must atomically consume this reservation under the same account/session locks and establish durable Map ownership. Expired unconsumed reservations are recoverable by Login; absence never authorizes deleting an existing Map session.';
