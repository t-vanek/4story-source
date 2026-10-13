-- Source TPostBillsUpdate and online SM_POSTBILL expiry. Offline SQL job and
-- World reserved/player producers are separate obligations, not replaced here.
ALTER TABLE app_world."TPOSTTABLE"
    ADD COLUMN recipient_bytes bytea,
    ADD COLUMN bill_due_at timestamptz,
    ADD CONSTRAINT post_recipient_bytes_shape CHECK (
        recipient_bytes IS NULL OR octet_length(recipient_bytes)<=50);
-- A return changes the post recipient and every attachment together. Defer the
-- existing recipient FK only inside that transaction; retain global item IDs.
ALTER TABLE app_world."TITEMTABLE"
    DROP CONSTRAINT item_post_recipient,
    ADD CONSTRAINT item_post_recipient FOREIGN KEY("bWorldID","dwOwnerID",post_id)
        REFERENCES app_world."TPOSTTABLE"("bWorldID","dwCharID","dwPostID")
        DEFERRABLE INITIALLY IMMEDIATE;
CREATE INDEX native_unread_bills ON app_world."TPOSTTABLE"("bWorldID","dwSendID","timeRecv")
    WHERE "bType"=2 AND "bRead"=0;
ALTER TABLE app_world.map_checkpoints ADD COLUMN mail_notification_baseline bigint NOT NULL DEFAULT 0
    CHECK(mail_notification_baseline>=0);

CREATE TABLE app_world.mail_notifications (
    notification_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id smallint NOT NULL,
    char_id integer NOT NULL,
    post_id integer NOT NULL,
    post_type smallint NOT NULL CHECK(post_type BETWEEN 0 AND 255),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    status text NOT NULL DEFAULT 'pending' CHECK(status IN ('pending','committed')),
    server_id smallint,
    owner_token text CHECK(owner_token ~ '^[0-9a-f]{64}$'),
    connection_id bigint CHECK(connection_id>0),
    authority_epoch bigint CHECK(authority_epoch>=0),
    session_key bigint,
    tutorial_suppressed boolean NOT NULL DEFAULT false,
    committed_at timestamptz,
    CHECK((status='pending' AND server_id IS NULL AND owner_token IS NULL
        AND connection_id IS NULL AND authority_epoch IS NULL AND session_key IS NULL AND committed_at IS NULL)
        OR (status='committed' AND server_id IS NOT NULL AND owner_token IS NOT NULL
        AND connection_id IS NOT NULL AND authority_epoch IS NOT NULL AND session_key IS NOT NULL AND committed_at IS NOT NULL)),
    FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE,
    FOREIGN KEY(world_id,post_id) REFERENCES app_world."TPOSTTABLE"("bWorldID","dwPostID") ON DELETE CASCADE
);
CREATE INDEX pending_mail_notifications ON app_world.mail_notifications(world_id,char_id,notification_id)
    WHERE status='pending';
REVOKE ALL ON app_world.mail_notifications FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.mail_notifications_notification_id_seq FROM PUBLIC;
CREATE TABLE reconstruction.mailbox_imports (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    character_mapping_sha256 text NOT NULL CHECK(character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    source_timezone text NOT NULL,
    source_varchar_encoding text NOT NULL,
    wire_encoding text NOT NULL,
    source_post_rows bigint NOT NULL CHECK(source_post_rows>=0),
    source_attachment_rows bigint NOT NULL CHECK(source_attachment_rows>=0),
    source_postitem_rows bigint NOT NULL CHECK(source_postitem_rows=0),
    imported_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(run_id,world_id)
);
REVOKE ALL ON reconstruction.mailbox_imports FROM PUBLIC;
COMMENT ON TABLE app_world.mail_notifications IS
    'Native durable replacement for transient POSTRECV routing. Only the fenced ready primary consumes a matching current recipient/type. Commit-before-send is not a client delivery ACK; a lost reply does not lose the mailbox. Offline notifications can be presented on reconnect.';
COMMENT ON COLUMN app_world."TPOSTTABLE".recipient_bytes IS
    'Original recipient wire bytes for lossless sender/recipient exchange on return/payment. NULL is not permission to invent a non-ASCII historical encoding.';
COMMENT ON COLUMN app_world."TPOSTTABLE".bill_due_at IS
    'Native producer deadline from live posting seconds plus POST_DURATION. NULL historical/previous rows use the original CTBLPostBill minute-rounded timeRecv plus three days.';
