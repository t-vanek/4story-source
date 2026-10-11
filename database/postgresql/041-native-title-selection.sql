-- Original owned-title selection, committed before World/client publication.
-- No historical backfill and no change to existing title flags or receipts.
CREATE TABLE app_world.title_selection_operations (
 selection_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 world_id smallint NOT NULL,
 char_id integer NOT NULL,
 previous_selection_id bigint NOT NULL CHECK(previous_selection_id>=0),
 session_key bigint NOT NULL CHECK(session_key BETWEEN 0 AND 4294967295),
 server_id smallint NOT NULL,
 owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
 connection_id bigint NOT NULL CHECK(connection_id>0),
 authority_epoch bigint NOT NULL CHECK(authority_epoch>=0),
 previous_title integer NOT NULL CHECK(previous_title BETWEEN 0 AND 65535),
 selected_title integer NOT NULL CHECK(selected_title BETWEEN 0 AND 65535),
 before_titles jsonb NOT NULL CHECK(jsonb_typeof(before_titles)='array'),
 after_titles jsonb NOT NULL CHECK(jsonb_typeof(after_titles)='array'),
 request bytea NOT NULL CHECK(octet_length(request)=2),
 selected_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 CHECK(selection_id>previous_selection_id),
 CHECK(previous_title<>selected_title),
 UNIQUE(world_id,char_id,previous_selection_id),
 FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE INDEX title_selection_character_latest ON app_world.title_selection_operations(world_id,char_id,selection_id DESC);
REVOKE ALL ON app_world.title_selection_operations FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.title_selection_operations_selection_id_seq FROM PUBLIC;
COMMENT ON TABLE app_world.title_selection_operations IS
 'Selection and its fenced fresh/full-graph checkpoint commit atomically. Same-title and unowned-title requests stay silent. World relays have the unchanged original body and are never replayed across a lost connection. Unknown database commit outcomes close the client without automatic retry.';
