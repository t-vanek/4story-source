-- Native World reserved-post delivery. Source TGetReservedPost + minute timer.
-- No historical migration is edited, applied or re-imported by this file.
CREATE TABLE app_world."TRESERVEDPOST" (
    queue_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwSeq" integer, -- exact historical identity, NULL for new native producers
    "dwRecverID" integer NOT NULL,
    "szSender" text NOT NULL,
    "szTitle" text NOT NULL,
    "szMessage" text NOT NULL,
    "bSend" smallint NOT NULL CHECK ("bSend" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
    "dwDuraCur" integer NOT NULL,
    "bRefineCur" smallint NOT NULL CHECK ("bRefineCur" BETWEEN 0 AND 255),
    "dEndTime" timestamp without time zone NOT NULL,
    "bGradeEffect" smallint NOT NULL CHECK ("bGradeEffect" BETWEEN 0 AND 255),
    "bMagic1" smallint NOT NULL CHECK ("bMagic1" BETWEEN 0 AND 255),
    "bMagic2" smallint NOT NULL CHECK ("bMagic2" BETWEEN 0 AND 255),
    "bMagic3" smallint NOT NULL CHECK ("bMagic3" BETWEEN 0 AND 255),
    "bMagic4" smallint NOT NULL CHECK ("bMagic4" BETWEEN 0 AND 255),
    "bMagic5" smallint NOT NULL CHECK ("bMagic5" BETWEEN 0 AND 255),
    "bMagic6" smallint NOT NULL CHECK ("bMagic6" BETWEEN 0 AND 255),
    "wValue1" smallint NOT NULL,
    "wValue2" smallint NOT NULL,
    "wValue3" smallint NOT NULL,
    "wValue4" smallint NOT NULL,
    "wValue5" smallint NOT NULL,
    "wValue6" smallint NOT NULL,
    "dwTime1" integer NOT NULL,
    "dwTime2" integer NOT NULL,
    "dwTime3" integer NOT NULL,
    "dwTime4" integer NOT NULL,
    "dwTime5" integer NOT NULL,
    "dwTime6" integer NOT NULL,
    "bGem" smallint NOT NULL CHECK ("bGem" BETWEEN 0 AND 255),
    "wMoggItemID" smallint NOT NULL,
    sender_bytes bytea NOT NULL CHECK(octet_length(sender_bytes)<=50),
    title_bytes bytea NOT NULL CHECK(octet_length(title_bytes)<=256),
    message_bytes bytea NOT NULL CHECK(octet_length(message_bytes)<=2048),
    delivered_post_id integer,
    delivered_item_id bigint,
    settled_at timestamptz,
    source_run_id bigint REFERENCES reconstruction.import_runs(id),
    source_row_number bigint CHECK(source_row_number>0),
    CHECK("bSend" IN (0,1,2)),
    CHECK((source_run_id IS NULL AND source_row_number IS NULL AND "dwSeq" IS NULL)
        OR (source_run_id IS NOT NULL AND source_row_number IS NOT NULL AND "dwSeq" IS NOT NULL)),
    UNIQUE("bWorldID",source_run_id,"dwSeq"),
    UNIQUE("bWorldID",source_run_id,source_row_number),
    CHECK((delivered_post_id IS NULL AND delivered_item_id IS NULL)
        OR ("bSend"=1 AND delivered_post_id>0 AND delivered_item_id>0 AND settled_at IS NOT NULL))
);
CREATE INDEX native_pending_reserved_mail ON app_world."TRESERVEDPOST"("bWorldID",queue_id) WHERE "bSend"=0;
CREATE TABLE app_world.reserved_mail_clock (
    world_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    last_minute bigint NOT NULL,
    updated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE reconstruction.reserved_mail_imports (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    character_mapping_sha256 text NOT NULL CHECK(character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    source_timezone text NOT NULL,
    source_varchar_encoding text NOT NULL,
    wire_encoding text NOT NULL,
    source_rows bigint NOT NULL CHECK(source_rows>=0),
    imported_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(run_id,world_id)
);
REVOKE ALL ON app_world."TRESERVEDPOST",app_world.reserved_mail_clock,reconstruction.reserved_mail_imports FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world."TRESERVEDPOST_queue_id_seq" FROM PUBLIC;
COMMENT ON TABLE app_world."TRESERVEDPOST" IS
    'Source fields preserved independently of native queue identity. World delivers at most30 per minute; missing recipient becomes bSend2 and stops this batch. Every delivered post/item/notification/status commits together. Source terminal rows do not invent old delivery IDs.';
COMMENT ON COLUMN app_world."TRESERVEDPOST".sender_bytes IS
    'Explicit original ANSI output profile, max50 bytes as TGetReservedPost output. Unrepresentable/truncated historical text requires reconciliation, never silent byte loss.';
COMMENT ON TABLE app_world.reserved_mail_clock IS
    'Durable native minute gate shared by concurrent/restarted World processes. Stable queue_id order replaces unspecified SQL Server TOP1. Not client metadata.';
