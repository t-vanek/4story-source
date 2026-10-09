-- Additive to immutable movement receipts from 027. Splits allocate from the
-- same transactional world high-water as Login starter items (migration 012).
CREATE TABLE app_world.inventory_stack_changes (
 operation_id bigint NOT NULL CHECK(operation_id>0),
 ordinal smallint NOT NULL CHECK(ordinal IN (0,1)),
 kind text NOT NULL CHECK(kind IN ('split','merge')),
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 state_contract smallint NOT NULL CHECK(state_contract IN (2,3)),
 item_id bigint NOT NULL,
 parent_id bigint NOT NULL,
 source_bag smallint NOT NULL CHECK(source_bag BETWEEN 0 AND 249 OR source_bag=255),
 source_slot smallint NOT NULL CHECK(source_slot BETWEEN 0 AND 254),
 destination_bag smallint NOT NULL CHECK(destination_bag BETWEEN 0 AND 249 OR destination_bag=255),
 destination_slot smallint NOT NULL CHECK(destination_slot BETWEEN 0 AND 254),
 before_count smallint NOT NULL CHECK(before_count BETWEEN 0 AND 255),
 after_count smallint NOT NULL CHECK(after_count BETWEEN 0 AND 255),
 before_hash text CHECK(before_hash ~ '^[0-9a-f]{64}$'),
 after_hash text CHECK(after_hash ~ '^[0-9a-f]{64}$'),
 before_graph_hash text CHECK(before_graph_hash ~ '^[0-9a-f]{64}$'),
 after_graph_hash text CHECK(after_graph_hash ~ '^[0-9a-f]{64}$'),
 core_fingerprint text NOT NULL CHECK(core_fingerprint ~ '^[0-9a-f]{64}$'),
 committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 PRIMARY KEY(operation_id,ordinal),
 UNIQUE(operation_id,item_id),
 CHECK(before_count+after_count>0),
 CHECK((before_count=0)=(before_hash IS NULL)),
 CHECK((after_count=0)=(after_hash IS NULL)),
 CHECK((kind='split' AND after_count>0 AND
       ((ordinal=0 AND item_id=parent_id AND before_count>after_count)
        OR (ordinal=1 AND item_id<>parent_id AND before_count=0)))
    OR (kind='merge' AND item_id=parent_id AND before_count>0 AND
       ((ordinal=0 AND after_count<=before_count) OR (ordinal=1 AND after_count>=before_count)))),
 CHECK((state_contract=2 AND before_graph_hash IS NOT NULL AND after_graph_hash IS NOT NULL)
    OR (state_contract=3 AND before_graph_hash IS NULL AND after_graph_hash IS NULL)),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
COMMENT ON TABLE app_world.inventory_stack_changes IS
 'Two ordered changes per split/merge transaction. Source ordinal 0 precedes destination 1; counts are conserved across the pair. A split has a fresh allocated ID linked to its source; merge deletion has no after hash. Full destinations preserve source no-op ACK behavior. Commit item rows or authoritative graph, allocator, core, timers and both receipts before sending packets; never retry an unknown commit.';
