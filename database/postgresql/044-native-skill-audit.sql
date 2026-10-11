-- Native replacement for TGAME_RAGEZONE.dbo.TLogSkill (default Map build,
-- DEF_UDPLOG undefined). Historical TSKILLLOG remains in legacy_game unchanged.
-- This new runtime log uses UTC wall time; historical GETDATE values are NOT
-- reinterpreted. Source DWORD/WORD arguments retain their signed SQL bit pattern.
CREATE TABLE app_world."TSKILLLOG" (
    log_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id smallint NOT NULL,
    purchase_id bigint NOT NULL UNIQUE CHECK (purchase_id > 0),
    "dwCharID" integer NOT NULL,
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "wSkill" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "timeInsert" timestamp without time zone NOT NULL
        DEFAULT (clock_timestamp() AT TIME ZONE 'UTC')
);
CREATE INDEX skill_log_character_time
    ON app_world."TSKILLLOG" (world_id, "dwCharID", "timeInsert", log_id);
REVOKE ALL ON app_world."TSKILLLOG" FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world."TSKILLLOG_log_id_seq" FROM PUBLIC;
COMMENT ON TABLE app_world."TSKILLLOG" IS
    'Original successful skill-learning audit, committed atomically with the purchase, currency and recovery checkpoint. No character/purchase lifetime FK: deleting a character must retain its audit. purchase_id identifies the accepted action, not a hash of request bytes; a later valid identical request is a new action. Historical logs remain in legacy_game.TSKILLLOG.';
