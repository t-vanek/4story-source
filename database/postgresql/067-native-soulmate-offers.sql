-- Source-shaped World replies are offers, not committed relationships. Map
-- consumes one offer with the NPC-call item, purse, directed link and checkpoint
-- in its existing native transaction. Historical migrations remain immutable.
CREATE TABLE app_world.soulmate_offers (
    offer_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    char_id integer NOT NULL,
    session_key integer NOT NULL,
    source_server smallint NOT NULL CHECK(source_server BETWEEN 1 AND 255),
    map_owner_token text NOT NULL CHECK(map_owner_token ~ '^[0-9a-f]{64}$'),
    connection_id bigint NOT NULL CHECK(connection_id>0),
    authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
    request_opcode integer NOT NULL CHECK(request_opcode IN (21532,21534)),
    request bytea NOT NULL,
    world_owner_token text CHECK(world_owner_token ~ '^[0-9a-f]{64}$'),
    response_opcode integer CHECK(response_opcode IN (37128,37130)),
    response bytea,
    target_id integer,
    target_name bytea,
    target_region bigint CHECK(target_region BETWEEN 0 AND 4294967295),
    target_level smallint CHECK(target_level BETWEEN 0 AND 255),
    target_class smallint CHECK(target_class BETWEEN 0 AND 255),
    target_session_key integer,
    target_server smallint CHECK(target_server BETWEEN 0 AND 255),
    status text NOT NULL DEFAULT 'pending' CHECK(status IN ('pending','prepared','consumed','cancelled','abandoned')),
    committed_operation_id bigint REFERENCES app_world.quest_operations(operation_id),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    prepared_at timestamptz,
    consumed_at timestamptz,
    FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE",
    FOREIGN KEY(world_id,target_id) REFERENCES app_world."TCHARTABLE",
    CHECK(status IN ('pending','abandoned') OR (world_owner_token IS NOT NULL AND response_opcode IS NOT NULL AND response IS NOT NULL
        AND target_name IS NOT NULL AND target_region IS NOT NULL AND target_level IS NOT NULL AND target_class IS NOT NULL
        AND target_session_key IS NOT NULL AND target_server IS NOT NULL AND prepared_at IS NOT NULL)),
    CHECK((status IN ('consumed','cancelled')) = (committed_operation_id IS NOT NULL AND consumed_at IS NOT NULL)),
    CHECK((request_opcode=21532 AND (response_opcode IS NULL OR response_opcode=37128))
       OR (request_opcode=21534 AND (response_opcode IS NULL OR response_opcode=37130)))
);
CREATE UNIQUE INDEX native_soulmate_one_pending ON app_world.soulmate_offers(world_id,char_id)
    WHERE status IN ('pending','prepared');
CREATE INDEX native_soulmate_committed ON app_world.soulmate_offers(world_id,offer_id)
    WHERE status='consumed';
COMMENT ON TABLE app_world.soulmate_offers IS
 'Server-only request/World selection receipts. Original CS/MW packet fields are unchanged. Equal bytes after consumption are a new invocation; an unknown commit is never replayed.';
REVOKE ALL ON app_world.soulmate_offers FROM PUBLIC;
