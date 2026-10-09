-- Additive equipment receipts; earlier carried-item receipts keep their exact
-- contracts. Every operation may displace multiple items before the requested
-- move, so record one header and a final per-identity diff in the same commit.
CREATE TABLE app_world.equipment_operations (
 operation_id bigint PRIMARY KEY CHECK(operation_id>0),
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 state_contract smallint NOT NULL CHECK(state_contract IN (2,3)),
 request bytea NOT NULL CHECK(octet_length(request)=5),
 changed_items integer NOT NULL CHECK(changed_items>=0),
 before_graph_hash text CHECK(before_graph_hash ~ '^[0-9a-f]{64}$'),
 after_graph_hash text CHECK(after_graph_hash ~ '^[0-9a-f]{64}$'),
 core_fingerprint text NOT NULL CHECK(core_fingerprint ~ '^[0-9a-f]{64}$'),
 committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK((state_contract=2 AND before_graph_hash IS NOT NULL AND after_graph_hash IS NOT NULL)
    OR (state_contract=3 AND before_graph_hash IS NULL AND after_graph_hash IS NULL)),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world.equipment_item_changes (
 operation_id bigint NOT NULL REFERENCES app_world.equipment_operations ON DELETE CASCADE,
 ordinal integer NOT NULL CHECK(ordinal>=0),
 item_id bigint NOT NULL CHECK(item_id<>0),
 parent_id bigint NOT NULL CHECK(parent_id<>0),
 source_bag smallint NOT NULL CHECK(source_bag BETWEEN 0 AND 249 OR source_bag IN (254,255)),
 source_slot smallint NOT NULL CHECK(source_slot BETWEEN 0 AND 254),
 destination_bag smallint NOT NULL CHECK(destination_bag BETWEEN 0 AND 249 OR destination_bag IN (254,255)),
 destination_slot smallint NOT NULL CHECK(destination_slot BETWEEN 0 AND 254),
 before_count smallint NOT NULL CHECK(before_count BETWEEN 0 AND 255),
 after_count smallint NOT NULL CHECK(after_count BETWEEN 0 AND 255),
 before_hash text CHECK(before_hash ~ '^[0-9a-f]{64}$'),
 after_hash text CHECK(after_hash ~ '^[0-9a-f]{64}$'),
 PRIMARY KEY(operation_id,ordinal),
 UNIQUE(operation_id,item_id),
 CHECK(before_count+after_count>0),
 CHECK((before_count=0)=(before_hash IS NULL)),
 CHECK((after_count=0)=(after_hash IS NULL)),
 CHECK(before_count>0 OR (item_id<>parent_id AND after_count=1))
);
REVOKE ALL ON app_world.equipment_operations,app_world.equipment_item_changes FROM PUBLIC;
COMMENT ON TABLE app_world.equipment_operations IS
 'Native equipment mutation, displacement, core HP/MP clamping, learned timers and recovery receipt commit atomically before original item/EQUIP/stat/HPMP replies. Graph state remains authoritative over normalized children. No automatic retry after uncertain commit.';
