-- Mutable, empty character state. No historical player rows are imported.
-- Multi-world state shares one PG transaction with app_global. SQL Server's
-- timestamp-without-zone sentinels remain literal 1900-01-01; NEW runtime clock
-- values use UTC, rounded to minutes where the source used SMALLDATETIME.
CREATE SCHEMA app_world;
CREATE TABLE app_world.worlds (
    group_id smallint PRIMARY KEY REFERENCES app_global."TGROUP"("bGroupID"),
    item_world smallint NOT NULL UNIQUE CHECK (item_world BETWEEN 0 AND 127),
    item_high_water bigint NOT NULL,
    CHECK (item_high_water >= item_world::bigint * 72057594037927936),
    CHECK (item_high_water < (item_world::numeric + 1) * 72057594037927936)
);
COMMENT ON TABLE app_world.worlds IS
 'Operator-provisioned normal worlds only. item_world is TGAME.TDBITEMINDEXTABLE.bWorld, not the client group ID. Backup world 0 high-water is 735812. Preserve that floor for this lineage; other worlds require reviewed allocation provenance. Values 128+ exceed original signed BIGINT positive range and are unsupported.';
ALTER TABLE app_global."TALLCHARTABLE"
    ADD COLUMN "szName" varchar(50),
    ADD COLUMN "dDeleteDate" timestamp without time zone;
CREATE UNIQUE INDEX all_char_name_ci ON app_global."TALLCHARTABLE"(lower("szName" COLLATE "C"));
ALTER TABLE app_global."TRESERVEDNAME" ADD COLUMN "dwUserID" integer REFERENCES app_global."TACCOUNT_PW";
COMMENT ON COLUMN app_global."TRESERVEDNAME"."dwUserID" IS
 'Matching owner can use reserved name; NULL reserves against everyone. No historical reservations imported.';
CREATE TABLE app_world."TCHARTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer GENERATED ALWAYS AS IDENTITY NOT NULL CHECK ("dwCharID">0),
    "dwUserID" integer NOT NULL,
    "bSlot" smallint NOT NULL CHECK ("bSlot" BETWEEN 0 AND 255),
    "szNAME" text NOT NULL,
    "bStartAct" smallint DEFAULT 1 NOT NULL CHECK ("bStartAct" BETWEEN 0 AND 255),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bRace" smallint NOT NULL CHECK ("bRace" BETWEEN 0 AND 255),
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bRealSex" smallint NOT NULL CHECK ("bRealSex" BETWEEN 0 AND 255),
    "bSex" smallint NOT NULL CHECK ("bSex" BETWEEN 0 AND 255),
    "bHair" smallint NOT NULL CHECK ("bHair" BETWEEN 0 AND 255),
    "bFace" smallint NOT NULL CHECK ("bFace" BETWEEN 0 AND 255),
    "bBody" smallint NOT NULL CHECK ("bBody" BETWEEN 0 AND 255),
    "bPants" smallint NOT NULL CHECK ("bPants" BETWEEN 0 AND 255),
    "bHand" smallint NOT NULL CHECK ("bHand" BETWEEN 0 AND 255),
    "bFoot" smallint NOT NULL CHECK ("bFoot" BETWEEN 0 AND 255),
    "bHelmetHide" smallint DEFAULT 0 NOT NULL CHECK ("bHelmetHide" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwEXP" integer NOT NULL,
    "dwHP" integer NOT NULL,
    "dwMP" integer NOT NULL,
    "wSkillPoint" smallint DEFAULT 0 NOT NULL,
    "dwRegion" integer DEFAULT 0 NOT NULL,
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "bGuildLeave" smallint DEFAULT 0 NOT NULL CHECK ("bGuildLeave" BETWEEN 0 AND 255),
    "dwGuildLeaveTime" integer DEFAULT 0 NOT NULL,
    "wMapID" smallint NOT NULL,
    "wSpawnID" smallint DEFAULT 0 NOT NULL,
    "wLastSpawnID" smallint DEFAULT 0 NOT NULL,
    "wTemptedMon" smallint NOT NULL,
    "bAftermath" smallint NOT NULL CHECK ("bAftermath" BETWEEN 0 AND 255),
    "fPosX" real NOT NULL,
    "fPosY" real NOT NULL,
    "fPosZ" real NOT NULL,
    "wDIR" smallint NOT NULL,
    "dwRankPoint" integer DEFAULT 0 NOT NULL,
    "bDelete" smallint DEFAULT 0 NOT NULL CHECK ("bDelete" BETWEEN 0 AND 255),
    "dCreateDate" timestamp without time zone DEFAULT date_trunc('minute', timezone('UTC', CURRENT_TIMESTAMP) + interval '30 seconds') NOT NULL,
    "dDeleteDate" timestamp without time zone,
    "dwLastDestination" integer DEFAULT 0 NOT NULL,
    "bOriCountry" smallint DEFAULT 0 NOT NULL CHECK ("bOriCountry" BETWEEN 0 AND 255),
    "dLogoutDate" timestamp without time zone DEFAULT TIMESTAMP '1900-01-01' NOT NULL,
    "bStatLevel" smallint CHECK ("bStatLevel" BETWEEN 0 AND 255),
    "bStatPoint" smallint CHECK ("bStatPoint" BETWEEN 0 AND 255),
    "dwStatExp" integer,
    PRIMARY KEY ("bWorldID","dwCharID"),
    FOREIGN KEY ("dwUserID") REFERENCES app_global."TACCOUNT_PW",
    CHECK ("bSlot" BETWEEN 0 AND 5)
);
CREATE TABLE app_world."TINVENTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "dEndTime" timestamp without time zone NOT NULL,
    "bELD" smallint DEFAULT 0 NOT NULL CHECK ("bELD" BETWEEN 0 AND 255),
    PRIMARY KEY ("bWorldID","dwCharID","bInvenID"),
    FOREIGN KEY ("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world."TTITLETABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "wTitleID" smallint NOT NULL,
    "bSelected" smallint NOT NULL CHECK ("bSelected" BETWEEN 0 AND 255),
    PRIMARY KEY ("bWorldID","dwCharID","wTitleID"),
    FOREIGN KEY ("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world."TCABINETTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "bCabinetID" smallint NOT NULL CHECK ("bCabinetID" BETWEEN 0 AND 255),
    "bUse" smallint NOT NULL CHECK ("bUse" BETWEEN 0 AND 255),
    PRIMARY KEY ("bWorldID","dwCharID","bCabinetID"),
    FOREIGN KEY ("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world."TSKILLTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "wSkillID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwRemainTick" integer DEFAULT 0 NOT NULL,
    PRIMARY KEY ("bWorldID","dwCharID","wSkillID"),
    FOREIGN KEY ("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world."THOTKEYTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "bType1" smallint NOT NULL CHECK ("bType1" BETWEEN 0 AND 255),
    "wID1" smallint NOT NULL,
    "bType2" smallint NOT NULL CHECK ("bType2" BETWEEN 0 AND 255),
    "wID2" smallint NOT NULL,
    "bType3" smallint NOT NULL CHECK ("bType3" BETWEEN 0 AND 255),
    "wID3" smallint NOT NULL,
    "bType4" smallint NOT NULL CHECK ("bType4" BETWEEN 0 AND 255),
    "wID4" smallint NOT NULL,
    "bType5" smallint NOT NULL CHECK ("bType5" BETWEEN 0 AND 255),
    "wID5" smallint NOT NULL,
    "bType6" smallint NOT NULL CHECK ("bType6" BETWEEN 0 AND 255),
    "wID6" smallint NOT NULL,
    "bType7" smallint NOT NULL CHECK ("bType7" BETWEEN 0 AND 255),
    "wID7" smallint NOT NULL,
    "bType8" smallint NOT NULL CHECK ("bType8" BETWEEN 0 AND 255),
    "wID8" smallint NOT NULL,
    "bType9" smallint NOT NULL CHECK ("bType9" BETWEEN 0 AND 255),
    "wID9" smallint NOT NULL,
    "bType10" smallint NOT NULL CHECK ("bType10" BETWEEN 0 AND 255),
    "wID10" smallint NOT NULL,
    "bType11" smallint NOT NULL CHECK ("bType11" BETWEEN 0 AND 255),
    "wID11" smallint NOT NULL,
    "bType12" smallint NOT NULL CHECK ("bType12" BETWEEN 0 AND 255),
    "wID12" smallint NOT NULL,
    PRIMARY KEY ("bWorldID","dwCharID","bInvenID"),
    FOREIGN KEY ("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world."TPOSTTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "dwPostID" integer GENERATED ALWAYS AS IDENTITY NOT NULL CHECK ("dwPostID">0),
    "szRecvName" text NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bRead" smallint NOT NULL CHECK ("bRead" BETWEEN 0 AND 255),
    "timeRecv" timestamp without time zone NOT NULL,
    "dwSendID" integer NOT NULL,
    "szSender" text NOT NULL,
    "szTitle" text NOT NULL,
    "szMessage" text NOT NULL,
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "bContain" smallint CHECK ("bContain" BETWEEN 0 AND 255),
    PRIMARY KEY ("bWorldID","dwPostID"),
    FOREIGN KEY ("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world."TRECALLMONTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwOwnerID" integer NOT NULL,
    "dwID" integer GENERATED ALWAYS AS IDENTITY NOT NULL CHECK ("dwID">0),
    "wMonID" smallint NOT NULL,
    "wPetID" smallint DEFAULT 0 NOT NULL,
    "dwATTR" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwHP" integer NOT NULL,
    "dwMP" integer NOT NULL,
    "bSkillLevel" smallint DEFAULT 0 NOT NULL CHECK ("bSkillLevel" BETWEEN 0 AND 255),
    "wPosX" smallint NOT NULL,
    "wPosY" smallint NOT NULL,
    "wPosZ" smallint NOT NULL,
    "dwTime" integer NOT NULL,
    "bEffect" smallint DEFAULT 0 NOT NULL CHECK ("bEffect" BETWEEN 0 AND 255),
    PRIMARY KEY ("bWorldID","dwID"),
    FOREIGN KEY ("bWorldID","dwOwnerID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world."TPETTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwUserID" integer NOT NULL,
    "wPetID" smallint NOT NULL,
    "szName" text NOT NULL,
    "timeUse" timestamp without time zone NOT NULL,
    "bEffect" smallint CHECK ("bEffect" BETWEEN 0 AND 255),
    PRIMARY KEY ("bWorldID","dwUserID","wPetID"),
    FOREIGN KEY ("dwUserID") REFERENCES app_global."TACCOUNT_PW"
);
CREATE TABLE app_world."TITEMTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dlID" bigint NOT NULL,
    "bStorageType" smallint NOT NULL CHECK ("bStorageType" BETWEEN 0 AND 255),
    "dwStorageID" integer NOT NULL,
    "bOwnerType" smallint NOT NULL CHECK ("bOwnerType" BETWEEN 0 AND 255),
    "dwOwnerID" integer NOT NULL,
    "bItemID" smallint NOT NULL CHECK ("bItemID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint DEFAULT 0 NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
    "dwDuraCur" integer NOT NULL,
    "bRefineCur" smallint NOT NULL CHECK ("bRefineCur" BETWEEN 0 AND 255),
    "dEndTime" timestamp without time zone NOT NULL,
    "bGradeEffect" smallint DEFAULT 0 NOT NULL CHECK ("bGradeEffect" BETWEEN 0 AND 255),
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
    "wCustomTex" integer NOT NULL DEFAULT 0 CHECK ("wCustomTex" BETWEEN 0 AND 65535),
    PRIMARY KEY ("bWorldID","dlID"),
    FOREIGN KEY ("bWorldID","dwOwnerID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE,
    CHECK ("bOwnerType"=0 AND "bStorageType"=0)
);
CREATE UNIQUE INDEX char_live_slot ON app_world."TCHARTABLE"("bWorldID","dwUserID","bSlot") WHERE "bDelete"=0;
CREATE UNIQUE INDEX char_name_ci ON app_world."TCHARTABLE"(lower("szNAME" COLLATE "C"));
CREATE INDEX char_account ON app_world."TCHARTABLE"("dwUserID","bWorldID");
CREATE UNIQUE INDEX item_slot ON app_world."TITEMTABLE"("bWorldID","dwOwnerID","dwStorageID","bItemID");
CREATE UNIQUE INDEX item_identity ON app_world."TITEMTABLE"("dlID");
CREATE TABLE app_world.guild_membership (
    world_id smallint NOT NULL,
    char_id integer NOT NULL,
    guild_id integer NOT NULL,
    fame integer NOT NULL DEFAULT 0,
    fame_color integer NOT NULL DEFAULT 0,
    PRIMARY KEY(world_id,char_id),
    FOREIGN KEY(world_id,char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
COMMENT ON TABLE app_world.guild_membership IS
 'Deletion and lobby projection boundary; full guild system remains pending. Runtime Login only reads this table.';
COMMENT ON COLUMN app_world."TRECALLMONTABLE"."dwID" IS
 'Modern identity repair: backup TCreateRecallMon expects generated dwID but its table has no identity/default.';
COMMENT ON COLUMN app_world."TITEMTABLE"."wCustomTex" IS
 'Explicit modern protocol extension; absent from backup. New starter items use no custom texture (0).';
REVOKE ALL ON SCHEMA app_world FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA app_world FROM PUBLIC;
REVOKE ALL ON ALL SEQUENCES IN SCHEMA app_world FROM PUBLIC;
