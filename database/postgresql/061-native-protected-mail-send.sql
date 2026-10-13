-- TProtectedInsert/Delete/SaveProtectedOption and player TPostCanSend/TSavePost.
-- Original archival objects and previously numbered migrations stay untouched.
ALTER TABLE app_world.mail_notifications
    ADD COLUMN received_at bigint,
    ADD COLUMN superseded_at timestamptz,
    ADD CONSTRAINT mail_notification_superseded_shape CHECK(superseded_at IS NULL OR status='pending');
CREATE INDEX current_pending_mail_notifications ON app_world.mail_notifications(world_id,char_id,notification_id)
    WHERE status='pending' AND superseded_at IS NULL;
COMMENT ON COLUMN app_world.mail_notifications.received_at IS
    'Original live POSTRECV second, independent of minute-rounded TPOST.timeRecv and later notification polling. NULL pre-upgrade receipts explicitly fall back to saved timeRecv.';
COMMENT ON COLUMN app_world.mail_notifications.superseded_at IS
    'Canceled original recipient/type notification after return or settlement; never replay against a later post generation. Pending is an undelivered receipt, not a client acknowledgement.';
CREATE TABLE app_world."TPROTECTEDTABLE" (
    "bWorldID" smallint NOT NULL,
    "dwCharID" integer NOT NULL,
    "dwProtected" integer NOT NULL,
    "szNAME" text NOT NULL,
    name_bytes bytea NOT NULL CHECK(octet_length(name_bytes) BETWEEN 1 AND 50),
    "bOption" smallint NOT NULL CHECK("bOption" BETWEEN 0 AND 255),
    PRIMARY KEY("bWorldID","dwCharID","dwProtected"),
    FOREIGN KEY("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
-- No target FK: source TDeleteChar removes only the owner's rows and leaves
-- other characters' protected entries (including their remembered name).
COMMENT ON TABLE app_world."TPROTECTEDTABLE" IS
    'Normalized authority on fresh load and Map transfer. Original remembered wire name and all option bits retained. Mail rejects any matching row regardless of option.';
CREATE TABLE app_world."TPOSTERRORTABLE" (
    error_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "dwGold" integer NOT NULL,
    post_id integer NOT NULL,
    created_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
COMMENT ON TABLE app_world."TPOSTERRORTABLE" IS
    'Native runtime audit of source TSavePost package type1 with signed dwGold>10; retained after post/character deletion. Historical rows remain in the complete archive.';
CREATE TABLE reconstruction.protected_imports (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    character_mapping_sha256 text NOT NULL CHECK(character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    source_varchar_encoding text NOT NULL,
    wire_encoding text NOT NULL,
    source_rows bigint NOT NULL CHECK(source_rows>=0),
    imported_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(run_id,world_id)
);
REVOKE ALL ON app_world."TPROTECTEDTABLE",app_world."TPOSTERRORTABLE",reconstruction.protected_imports FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world."TPOSTERRORTABLE_error_id_seq" FROM PUBLIC;
