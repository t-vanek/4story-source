-- Immediate, fenced single-reagent consumption. No historical catalog writes.
CREATE FUNCTION app_world.item_fingerprint(i app_world."TITEMTABLE")
RETURNS text LANGUAGE sql IMMUTABLE STRICT AS $$
 SELECT encode(sha256(convert_to(to_jsonb(i)::text,'UTF8')),'hex')
$$;
REVOKE ALL ON FUNCTION app_world.item_fingerprint(app_world."TITEMTABLE") FROM PUBLIC;
CREATE TABLE app_world.skill_item_consumptions (
 consumption_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 skill_id integer NOT NULL CHECK(skill_id BETWEEN 1 AND 65535),
 item_id bigint NOT NULL,
 before_count smallint NOT NULL CHECK(before_count BETWEEN 1 AND 255),
 after_count smallint NOT NULL CHECK(after_count=before_count-1),
 before_hash text NOT NULL CHECK(before_hash ~ '^[0-9a-f]{64}$'),
 after_hash text CHECK(after_hash ~ '^[0-9a-f]{64}$'),
 core_fingerprint text NOT NULL CHECK(core_fingerprint ~ '^[0-9a-f]{64}$'),
 committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK((after_count=0 AND after_hash IS NULL) OR (after_count>0 AND after_hash IS NOT NULL)),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
COMMENT ON TABLE app_world.skill_item_consumptions IS
 'Append-only native runtime receipts. One bag reagent decrement/delete, core HP/MP, learned timers and recovery checkpoint commit in one transaction before any client success/item ACK. Fresh primary only; no retry after unknown commit outcome. Character deletion cascades receipts.';
