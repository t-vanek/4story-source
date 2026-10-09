-- Keep slot uniqueness, but permit an atomic two-item swap inside one transaction.
ALTER TABLE app_world."TITEMTABLE"
 ADD CONSTRAINT item_slot UNIQUE USING INDEX item_slot DEFERRABLE INITIALLY IMMEDIATE;

CREATE TABLE app_world.inventory_movements (
 movement_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 operation_id bigint NOT NULL CHECK(operation_id>0),
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 state_contract smallint NOT NULL CHECK(state_contract IN (2,3)),
 item_id bigint NOT NULL,
 source_bag smallint NOT NULL CHECK(source_bag BETWEEN 0 AND 249 OR source_bag=255),
 source_slot smallint NOT NULL CHECK(source_slot BETWEEN 0 AND 254),
 destination_bag smallint NOT NULL CHECK(destination_bag BETWEEN 0 AND 249 OR destination_bag=255),
 destination_slot smallint NOT NULL CHECK(destination_slot BETWEEN 0 AND 254),
 item_count smallint NOT NULL CHECK(item_count BETWEEN 1 AND 255),
 before_hash text NOT NULL CHECK(before_hash ~ '^[0-9a-f]{64}$'),
 after_hash text NOT NULL CHECK(after_hash ~ '^[0-9a-f]{64}$'),
 before_graph_hash text CHECK(before_graph_hash ~ '^[0-9a-f]{64}$'),
 after_graph_hash text CHECK(after_graph_hash ~ '^[0-9a-f]{64}$'),
 core_fingerprint text NOT NULL CHECK(core_fingerprint ~ '^[0-9a-f]{64}$'),
 committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK(source_bag<>destination_bag OR source_slot<>destination_slot),
 CHECK((state_contract=2 AND before_graph_hash IS NOT NULL AND after_graph_hash IS NOT NULL)
    OR (state_contract=3 AND before_graph_hash IS NULL AND after_graph_hash IS NULL)),
 UNIQUE(operation_id,item_id),
 UNIQUE(operation_id,source_bag,source_slot),
 UNIQUE(operation_id,destination_bag,destination_slot),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
COMMENT ON TABLE app_world.inventory_movements IS
 'Append-only whole-stack relocation and different-template swap receipts. Item identity/count/attributes are conserved; all positions, core, timers and recovery state commit before original private item responses. Complete graphs remain authoritative over stale child tables. No retry after unknown commit outcome.';
