-- Native CheckItemEndTime -> POSTINVENITEM. Existing archives/migrations remain
-- unchanged. This does not claim the full player mail/return/billing system.
ALTER TABLE runtime_control.quest_condition_catalog
    ADD COLUMN server_messages boolean NOT NULL DEFAULT false;
ALTER TABLE runtime_control.quest_condition_activations
    ADD COLUMN server_messages boolean NOT NULL DEFAULT false;
CREATE VIEW game_compat.server_message_release AS
    SELECT r.* FROM game_compat.quest_reward_release r
        JOIN runtime_control.quest_condition_catalog c ON c.run_id=r.run_id
        WHERE c.singleton AND c.server_messages;
CREATE VIEW game_compat.server_messages AS
    SELECT s."dwID",s."szMessage",b.source_bytes AS message_bytes
    FROM legacy_game."TSVRMSGCHART" s
    JOIN game_compat.server_message_release r ON r.run_id=s."_import_run_id"
    JOIN reconstruction.source_text_bytes b ON b.run_id=s."_import_run_id"
        AND b.source_database='TGAME_RAGEZONE' AND b.source_schema='dbo'
        AND b.source_table='TSVRMSGCHART' AND b.source_row_number=s."_source_row_number"
        AND b.column_name='szMessage';
REVOKE ALL ON game_compat.server_message_release,game_compat.server_messages FROM PUBLIC;

ALTER TABLE app_world."TPOSTTABLE"
    ADD COLUMN sender_bytes bytea,
    ADD COLUMN title_bytes bytea,
    ADD COLUMN message_bytes bytea,
    ADD CONSTRAINT post_source_bytes_shape CHECK (
        (sender_bytes IS NULL AND title_bytes IS NULL AND message_bytes IS NULL) OR
        (sender_bytes IS NOT NULL AND title_bytes IS NOT NULL AND message_bytes IS NOT NULL
            AND octet_length(sender_bytes)<=50 AND octet_length(title_bytes)<=256
            AND octet_length(message_bytes)<=2048)),
    ADD CONSTRAINT post_recipient_identity UNIQUE("bWorldID","dwCharID","dwPostID");
-- Retain one global item identity, including graph-owned attachments. A post ID
-- can equal an inventory ID without making their independent slots collide.
ALTER TABLE app_world."TITEMTABLE"
    DROP CONSTRAINT "TITEMTABLE_check",
    DROP CONSTRAINT item_slot,
    ADD COLUMN post_id integer,
    ADD CONSTRAINT item_native_storage CHECK (
        "bOwnerType"=0 AND (("bStorageType"=0 AND post_id IS NULL) OR
        ("bStorageType"=2 AND post_id IS NOT NULL AND post_id="dwStorageID"))),
    ADD CONSTRAINT item_slot UNIQUE("bWorldID","dwOwnerID","bStorageType","dwStorageID","bItemID")
        DEFERRABLE INITIALLY IMMEDIATE,
    ADD CONSTRAINT item_post_recipient FOREIGN KEY("bWorldID","dwOwnerID",post_id)
        REFERENCES app_world."TPOSTTABLE"("bWorldID","dwCharID","dwPostID");
-- post_id duplicates the fenced storage ID. Do not invalidate existing item
-- fingerprints merely by adding a NULL ownership column during an upgrade.
CREATE OR REPLACE FUNCTION app_world.item_fingerprint(i app_world."TITEMTABLE")
RETURNS text LANGUAGE sql IMMUTABLE STRICT AS $$
    SELECT encode(sha256(convert_to((to_jsonb(i)-'post_id')::text,'UTF8')),'hex')
$$;
COMMENT ON COLUMN app_world."TITEMTABLE".post_id IS
    'Native attachment ownership; matches STORAGE_POST dwStorageID and recipient. Not a protocol field. No attachment is silently deleted with its post.';
COMMENT ON VIEW game_compat.server_messages IS
    'Complete pinned TSVRMSGCHART publication including original varchar bytes; no synthesized operator name or expired-bag message.';
