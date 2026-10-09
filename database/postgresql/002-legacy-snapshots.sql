-- Generated from restored metadata SHA256 62fc3fe7cb2ae38a194cd402b508eea84d9892b85e877f3082a5d90d75278f21.
-- Historical snapshot layer; not a drop-in game-server schema.


CREATE SCHEMA "legacy_game";

CREATE SCHEMA "legacy_game_tgame";

CREATE SCHEMA "legacy_global";

CREATE TABLE "legacy_global"."IPBLACKLIST" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "szIP" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."IPBLACKLIST_game" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "szIP" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TACCOUNT" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "szUserID" text,
    "szPasswd" text,
    "bCheck" smallint CHECK ("bCheck" BETWEEN 0 AND 255),
    "dFirstLogin" timestamp without time zone,
    "dLastLogin" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID")
);

CREATE TABLE "legacy_global"."TACCOUNT_PW" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "szUserID" text NOT NULL,
    "szPasswd" text,
    "bCheck" smallint CHECK ("bCheck" BETWEEN 0 AND 255),
    "dFirstLogin" timestamp without time zone,
    "dLastLogin" timestamp without time zone,
    "dwUserID" integer NOT NULL,
    "dwChanged" integer,
    "dwPWKey" text,
    "szRealUsername" text,
    "szOrigin" text,
    "dwEMKey" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID")
);

CREATE TABLE "legacy_global"."TALLCHARTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "bWorldID" smallint NOT NULL CHECK ("bWorldID" BETWEEN 0 AND 255),
    "dwCharID" integer NOT NULL,
    "bSlot" smallint NOT NULL CHECK ("bSlot" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bRace" smallint NOT NULL CHECK ("bRace" BETWEEN 0 AND 255),
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bSex" smallint NOT NULL CHECK ("bSex" BETWEEN 0 AND 255),
    "bHair" smallint NOT NULL CHECK ("bHair" BETWEEN 0 AND 255),
    "bFace" smallint NOT NULL CHECK ("bFace" BETWEEN 0 AND 255),
    "bBody" smallint NOT NULL CHECK ("bBody" BETWEEN 0 AND 255),
    "bPants" smallint NOT NULL CHECK ("bPants" BETWEEN 0 AND 255),
    "bHand" smallint NOT NULL CHECK ("bHand" BETWEEN 0 AND 255),
    "bFoot" smallint NOT NULL CHECK ("bFoot" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwEXP" integer NOT NULL,
    "bDelete" smallint NOT NULL CHECK ("bDelete" BETWEEN 0 AND 255),
    "dCreateDate" timestamp without time zone NOT NULL,
    "dDeleteDate" timestamp without time zone,
    "dLoginDate" timestamp without time zone,
    "dLogoutDate" timestamp without time zone,
    "dwPlayTime" integer NOT NULL,
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "isOnline" integer,
    "Ban" integer,
    "BanReason" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwSeq")
);

CREATE TABLE "legacy_global"."TALLCHARTABLE_TRIGGER" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "szDBOP" text NOT NULL,
    "dwSeq" integer NOT NULL,
    "dOPDate" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TALLLOCALTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dDate" timestamp without time zone NOT NULL,
    "bWorld" smallint NOT NULL CHECK ("bWorld" BETWEEN 0 AND 255),
    "wLocalID" smallint NOT NULL,
    "szLocalName" text NOT NULL,
    "bCountry" smallint CHECK ("bCountry" BETWEEN 0 AND 255),
    "dwGuild" integer,
    "szGuildName" text,
    "dOccupy" timestamp without time zone,
    "dDefend" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dDate", "bWorld", "wLocalID")
);

CREATE TABLE "legacy_global"."TALLRANKGUILDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "dDate" timestamp without time zone NOT NULL,
    "bRankType" smallint NOT NULL CHECK ("bRankType" BETWEEN 0 AND 255),
    "bWorld" smallint NOT NULL CHECK ("bWorld" BETWEEN 0 AND 255),
    "dwGuildSeq" integer NOT NULL,
    "szGuildName" text NOT NULL,
    "dwChief" integer NOT NULL,
    "szChiefName" text NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwExp" integer NOT NULL,
    "dwMemberCount" integer NOT NULL,
    "timeEstablish" timestamp without time zone NOT NULL,
    "dwPlayTime" integer NOT NULL,
    "nRank" integer,
    "nRankUpDown" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TALLRANKTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "dDate" timestamp without time zone NOT NULL,
    "bWorld" smallint NOT NULL CHECK ("bWorld" BETWEEN 0 AND 255),
    "bRankType" smallint NOT NULL CHECK ("bRankType" BETWEEN 0 AND 255),
    "dwUserID" integer,
    "dwCharID" integer NOT NULL,
    "nRank" integer NOT NULL,
    "nRankUpDown" integer,
    "szCharName" text NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bRace" smallint NOT NULL CHECK ("bRace" BETWEEN 0 AND 255),
    "bSex" smallint NOT NULL CHECK ("bSex" BETWEEN 0 AND 255),
    "bLevel" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dDate", "bWorld", "bRankType", "dwCharID")
);

CREATE TABLE "legacy_global"."TCABINETITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bWorldID" smallint NOT NULL CHECK ("bWorldID" BETWEEN 0 AND 255),
    "dwCharID" integer NOT NULL,
    "dwPostID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TCASHBONUSITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wCashItemID" smallint NOT NULL,
    "wBonusItem" smallint NOT NULL,
    "bBonusItemCount" smallint NOT NULL CHECK ("bBonusItemCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wCashItemID", "wBonusItem")
);

CREATE TABLE "legacy_global"."TCASHCATEGORYCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bID" smallint NOT NULL CHECK ("bID" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "wOrder" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bID")
);

CREATE TABLE "legacy_global"."TCASHGAMBLECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "dwProb" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
    "bGradeEffect" smallint NOT NULL CHECK ("bGradeEffect" BETWEEN 0 AND 255),
    "bMagic1" smallint NOT NULL CHECK ("bMagic1" BETWEEN 0 AND 255),
    "wValue1" smallint NOT NULL,
    "dwTime1" integer NOT NULL,
    "bMagic2" smallint NOT NULL CHECK ("bMagic2" BETWEEN 0 AND 255),
    "wValue2" smallint NOT NULL,
    "dwTime2" integer NOT NULL,
    "bMagic3" smallint NOT NULL CHECK ("bMagic3" BETWEEN 0 AND 255),
    "wValue3" smallint NOT NULL,
    "dwTime3" integer NOT NULL,
    "bMagic4" smallint NOT NULL CHECK ("bMagic4" BETWEEN 0 AND 255),
    "wValue4" smallint NOT NULL,
    "dwTime4" integer NOT NULL,
    "bMagic5" smallint NOT NULL CHECK ("bMagic5" BETWEEN 0 AND 255),
    "wValue5" smallint NOT NULL,
    "dwTime5" integer NOT NULL,
    "bMagic6" smallint NOT NULL CHECK ("bMagic6" BETWEEN 0 AND 255),
    "wValue6" smallint NOT NULL,
    "dwTime6" integer NOT NULL,
    "wGroup" smallint NOT NULL,
    "wUseTime" smallint NOT NULL,
    "dwDuraMax" integer NOT NULL,
    "dwDuraCur" integer NOT NULL,
    "bRefineCur" smallint NOT NULL CHECK ("bRefineCur" BETWEEN 0 AND 255),
    "bGem" smallint CHECK ("bGem" BETWEEN 0 AND 255),
    "wMoggItemID" smallint,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_global"."TCASHITEMBUYTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dDate" timestamp without time zone NOT NULL,
    "dwUserID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bChartType" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TCASHITEMCABINETTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "dwUserID" integer NOT NULL,
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
    "bWorldID" smallint NOT NULL,
    "dlID" bigint NOT NULL,
    "bGem" smallint NOT NULL CHECK ("bGem" BETWEEN 0 AND 255),
    "wMoggItemID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_global"."TCASHITEMCABINETTABLE_PW" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "dwUserID" integer NOT NULL,
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
    "bWorldID" smallint NOT NULL,
    "dlID" bigint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_global"."TCASHITEMCABINETTABLE_temp" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "dwUserID" integer NOT NULL,
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
    "bWorldID" smallint NOT NULL,
    "dlID" bigint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TCASHITEMINFOCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wCashItemID" smallint NOT NULL,
    "szName" text NOT NULL,
    "szUseTime" text NOT NULL,
    "szExplain" text NOT NULL,
    "bSellType" smallint NOT NULL CHECK ("bSellType" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wCashItemID")
);

CREATE TABLE "legacy_global"."TCASHITEMPRICETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bPackage" smallint NOT NULL CHECK ("bPackage" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bAmount" smallint NOT NULL CHECK ("bAmount" BETWEEN 0 AND 255),
    "fPrice" double precision NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TCASHSHOPITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "szName" text NOT NULL,
    "dwMoney" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "wInfoID" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
    "dwDuraCur" integer NOT NULL,
    "bRefineCur" smallint NOT NULL CHECK ("bRefineCur" BETWEEN 0 AND 255),
    "wUseTime" smallint NOT NULL,
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
    "bCanSell" smallint NOT NULL CHECK ("bCanSell" BETWEEN 0 AND 255),
    "bCategory" smallint NOT NULL CHECK ("bCategory" BETWEEN 0 AND 255),
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    "wOrder" smallint NOT NULL,
    "bSaleValue" smallint NOT NULL CHECK ("bSaleValue" BETWEEN 0 AND 255),
    "dwQuantity" integer NOT NULL,
    "wIconID" smallint,
    "bLimitedType" smallint CHECK ("bLimitedType" BETWEEN 0 AND 255),
    "dLimitedEnd" timestamp without time zone,
    "bItemCountry" smallint CHECK ("bItemCountry" BETWEEN 0 AND 255),
    "dwClassID" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_global"."TCASHTESTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "dwCash" integer NOT NULL,
    "dwBonus" integer NOT NULL,
    "isPosted" timestamp without time zone,
    "byWho" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID")
);

CREATE TABLE "legacy_global"."TCHANNEL" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "bChannel" smallint NOT NULL CHECK ("bChannel" BETWEEN 0 AND 255),
    "szNAME" text NOT NULL,
    "wFull" smallint NOT NULL,
    "wBusy" smallint NOT NULL,
    "bStatus" smallint NOT NULL CHECK ("bStatus" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bChannel", "bGroupID")
);

CREATE TABLE "legacy_global"."TCURRENTUSER" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwKEY" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "bChannel" smallint NOT NULL CHECK ("bChannel" BETWEEN 0 AND 255),
    "szIPAddr" text,
    "wPort" smallint NOT NULL,
    "bLocked" smallint NOT NULL CHECK ("bLocked" BETWEEN 0 AND 255),
    "dLoginDate" timestamp without time zone NOT NULL,
    "dEnterDate" timestamp without time zone NOT NULL,
    "dwPcBangID" integer NOT NULL,
    "bLuckyNumber" smallint NOT NULL CHECK ("bLuckyNumber" BETWEEN 0 AND 255),
    "szLoginIP" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwKEY")
);

CREATE TABLE "legacy_global"."TDURINGITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwRemainTime" integer NOT NULL,
    "dEndTime" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "wItemID")
);

CREATE TABLE "legacy_global"."TEMPUSERID" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TEVENT042501TL" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "USER010_Serial" integer NOT NULL,
    "EVENT042501_Cupon" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TEVENTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwIndex" integer NOT NULL,
    "bID" smallint NOT NULL CHECK ("bID" BETWEEN 0 AND 255),
    "szTitle" text NOT NULL,
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "bSvrType" smallint NOT NULL CHECK ("bSvrType" BETWEEN 0 AND 255),
    "bSvrID" smallint NOT NULL CHECK ("bSvrID" BETWEEN 0 AND 255),
    "dStartDate" timestamp without time zone NOT NULL,
    "dEndDate" timestamp without time zone NOT NULL,
    "wValue" smallint NOT NULL,
    "dwStartAlarm" integer NOT NULL,
    "dwEndAlarm" integer NOT NULL,
    "szStartMsg" text NOT NULL,
    "szEndMsg" text NOT NULL,
    "szValue" text NOT NULL,
    "wMapID" smallint NOT NULL,
    "szMidMsg" text NOT NULL,
    "bPartTime" smallint NOT NULL CHECK ("bPartTime" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwIndex")
);

CREATE TABLE "legacy_global"."TEVENTGIVEERROR" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bWorld" smallint NOT NULL CHECK ("bWorld" BETWEEN 0 AND 255),
    "dwCharID" integer NOT NULL,
    "wCount" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TEVENTHORSE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bWorld" smallint NOT NULL CHECK ("bWorld" BETWEEN 0 AND 255),
    "dwCharID" integer NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TGROUP" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "szNAME" text NOT NULL,
    "szDSN" text NOT NULL,
    "szUserID" text NOT NULL,
    "szPasswd" text,
    "wFull" smallint NOT NULL,
    "wBusy" smallint NOT NULL,
    "dwMaxUser" integer NOT NULL,
    "bUseRate" smallint NOT NULL CHECK ("bUseRate" BETWEEN 0 AND 255),
    "bStatus" smallint NOT NULL CHECK ("bStatus" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bGroupID")
);

CREATE TABLE "legacy_global"."TIPADDR" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMachineID" smallint NOT NULL CHECK ("bMachineID" BETWEEN 0 AND 255),
    "szIPAddr" text NOT NULL,
    "szPriAddr" text NOT NULL,
    "bActive" smallint NOT NULL CHECK ("bActive" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bMachineID", "szIPAddr")
);

CREATE TABLE "legacy_global"."TIPAUTHORITY" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "szIP" text NOT NULL,
    "bAuthority" smallint NOT NULL CHECK ("bAuthority" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "szIP")
);

CREATE TABLE "legacy_global"."TITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wItemID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    "szNAME" text NOT NULL,
    "wAttrID" smallint NOT NULL,
    "wUseValue" smallint NOT NULL,
    "dwSlotID" integer NOT NULL,
    "dwClassID" integer NOT NULL,
    "bPrmSlotID" smallint NOT NULL CHECK ("bPrmSlotID" BETWEEN 0 AND 255),
    "bSubSlotID" smallint NOT NULL CHECK ("bSubSlotID" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "fPrice" double precision NOT NULL,
    "bIsSell" smallint NOT NULL CHECK ("bIsSell" BETWEEN 0 AND 255),
    "bMinRange" smallint NOT NULL CHECK ("bMinRange" BETWEEN 0 AND 255),
    "bMaxRange" smallint NOT NULL CHECK ("bMaxRange" BETWEEN 0 AND 255),
    "bStack" smallint NOT NULL CHECK ("bStack" BETWEEN 0 AND 255),
    "bEquipSkill" smallint NOT NULL CHECK ("bEquipSkill" BETWEEN 0 AND 255),
    "bSlotCount" smallint NOT NULL CHECK ("bSlotCount" BETWEEN 0 AND 255),
    "bUseItemKind" smallint NOT NULL CHECK ("bUseItemKind" BETWEEN 0 AND 255),
    "bUseItemCount" smallint NOT NULL CHECK ("bUseItemCount" BETWEEN 0 AND 255),
    "bGrade" smallint NOT NULL CHECK ("bGrade" BETWEEN 0 AND 255),
    "wUseTime" smallint NOT NULL,
    "bUseType" smallint NOT NULL CHECK ("bUseType" BETWEEN 0 AND 255),
    "bCanGrade" smallint NOT NULL CHECK ("bCanGrade" BETWEEN 0 AND 255),
    "bCanMagic" smallint NOT NULL CHECK ("bCanMagic" BETWEEN 0 AND 255),
    "bCanRare" smallint NOT NULL CHECK ("bCanRare" BETWEEN 0 AND 255),
    "bDropLevel" smallint NOT NULL CHECK ("bDropLevel" BETWEEN 0 AND 255),
    "dwSpeedInc" integer NOT NULL,
    "bItemCountry" smallint NOT NULL CHECK ("bItemCountry" BETWEEN 0 AND 255),
    "bIsSpecial" smallint NOT NULL CHECK ("bIsSpecial" BETWEEN 0 AND 255),
    "dwDelay" integer NOT NULL,
    "fRevision" double precision NOT NULL,
    "fMRevision" double precision NOT NULL,
    "fAtRate" double precision NOT NULL,
    "fMAtRate" double precision NOT NULL,
    "bCanGamble" smallint NOT NULL CHECK ("bCanGamble" BETWEEN 0 AND 255),
    "wItemProb_G" smallint NOT NULL,
    "bDestroyProb" smallint NOT NULL CHECK ("bDestroyProb" BETWEEN 0 AND 255),
    "bGambleProb" smallint NOT NULL CHECK ("bGambleProb" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
    "bRefineMax" smallint NOT NULL CHECK ("bRefineMax" BETWEEN 0 AND 255),
    "bCanRepair" smallint NOT NULL CHECK ("bCanRepair" BETWEEN 0 AND 255),
    "wDelayGroupID" smallint NOT NULL,
    "bCanColor" smallint NOT NULL CHECK ("bCanColor" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TKEEPINGNAME" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "szName" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TLIMITEDLEVELCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMaxLevel" smallint NOT NULL CHECK ("bMaxLevel" BETWEEN 0 AND 255),
    "bNation" smallint NOT NULL CHECK ("bNation" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TLOG" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwKEY" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "bChannel" smallint NOT NULL CHECK ("bChannel" BETWEEN 0 AND 255),
    "timeLOGIN" timestamp without time zone NOT NULL,
    "timeLOGOUT" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwKEY")
);

CREATE TABLE "legacy_global"."TLogTest" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "szLOG" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_global"."TMACHINE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMachineID" smallint NOT NULL CHECK ("bMachineID" BETWEEN 0 AND 255),
    "szNAME" text NOT NULL,
    "bRouteID" smallint NOT NULL CHECK ("bRouteID" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bMachineID")
);

CREATE TABLE "legacy_global"."TMANAGER" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "szID" text NOT NULL,
    "szPasswd" text NOT NULL,
    "bOPAuthority" smallint NOT NULL CHECK ("bOPAuthority" BETWEEN 0 AND 255),
    "bAuthority" smallint NOT NULL CHECK ("bAuthority" BETWEEN 0 AND 255),
    "szName" text,
    "szPhoneNum" text,
    "szOpratorCharID" text,
    "dCreateDate" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "szID")
);

CREATE TABLE "legacy_global"."TMANAGERLOG" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "dDate" timestamp without time zone NOT NULL,
    "szIP" text NOT NULL,
    "szGMID" text NOT NULL,
    "szCommand" text NOT NULL,
    "szLog" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwSeq")
);

CREATE TABLE "legacy_global"."TMONSTERCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "szName" text NOT NULL,
    "bRace" smallint NOT NULL CHECK ("bRace" BETWEEN 0 AND 255),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "wKind" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bAIType" smallint NOT NULL CHECK ("bAIType" BETWEEN 0 AND 255),
    "bRange" smallint NOT NULL CHECK ("bRange" BETWEEN 0 AND 255),
    "wChaseRange" smallint NOT NULL,
    "bRoamProb" smallint NOT NULL CHECK ("bRoamProb" BETWEEN 0 AND 255),
    "bMoneyProb" smallint NOT NULL CHECK ("bMoneyProb" BETWEEN 0 AND 255),
    "dwMinMoney" integer NOT NULL,
    "dwMaxMoney" integer NOT NULL,
    "bItemProb" smallint NOT NULL CHECK ("bItemProb" BETWEEN 0 AND 255),
    "bDropCount" smallint NOT NULL CHECK ("bDropCount" BETWEEN 0 AND 255),
    "wExp" smallint NOT NULL,
    "bIsSelf" smallint NOT NULL CHECK ("bIsSelf" BETWEEN 0 AND 255),
    "bRecallType" smallint NOT NULL CHECK ("bRecallType" BETWEEN 0 AND 255),
    "bCanSelect" smallint NOT NULL CHECK ("bCanSelect" BETWEEN 0 AND 255),
    "bCanAttack" smallint NOT NULL CHECK ("bCanAttack" BETWEEN 0 AND 255),
    "bTame" smallint NOT NULL CHECK ("bTame" BETWEEN 0 AND 255),
    "bCall" smallint NOT NULL CHECK ("bCall" BETWEEN 0 AND 255),
    "bIsSpecial" smallint NOT NULL CHECK ("bIsSpecial" BETWEEN 0 AND 255),
    "bRemove" smallint NOT NULL CHECK ("bRemove" BETWEEN 0 AND 255),
    "wMonAttr" smallint NOT NULL,
    "wSummonAttr" smallint NOT NULL,
    "wTransSkillID" smallint NOT NULL,
    "fSize" double precision NOT NULL,
    "wSkill1" smallint NOT NULL,
    "wSkill2" smallint NOT NULL,
    "wSkill3" smallint NOT NULL,
    "wSkill4" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TNETWORK" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMachineID" smallint NOT NULL CHECK ("bMachineID" BETWEEN 0 AND 255),
    "szNetwork" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bMachineID")
);

CREATE TABLE "legacy_global"."TPCBANGMASTERTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwPcBangID" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "dWorld1" timestamp without time zone,
    "dWorld2" timestamp without time zone,
    "dWorld3" timestamp without time zone,
    "dWorld4" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwPcBangID")
);

CREATE TABLE "legacy_global"."TPCBANGPLAYTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "dwPlayDate" integer NOT NULL,
    "dwPlayTime" integer NOT NULL,
    "bItemCnt" smallint NOT NULL CHECK ("bItemCnt" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "dwPlayDate")
);

CREATE TABLE "legacy_global"."TPREVERSION" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwBetaVer" integer NOT NULL,
    "szPath" text NOT NULL,
    "szName" text NOT NULL,
    "dwSize" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwBetaVer")
);

CREATE TABLE "legacy_global"."TREGIONCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "szName" text NOT NULL,
    "wCountryID" smallint NOT NULL,
    "bCanfly" smallint NOT NULL CHECK ("bCanfly" BETWEEN 0 AND 255),
    "bLocal" smallint NOT NULL CHECK ("bLocal" BETWEEN 0 AND 255),
    "fDPosX" double precision NOT NULL,
    "fDPosY" double precision NOT NULL,
    "fDPosZ" double precision NOT NULL,
    "fCPosX" double precision NOT NULL,
    "fCPosY" double precision NOT NULL,
    "fCPosZ" double precision NOT NULL,
    "fBPosX" double precision NOT NULL,
    "fBPosY" double precision NOT NULL,
    "fBPosZ" double precision NOT NULL,
    "bCanMail" smallint NOT NULL CHECK ("bCanMail" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_global"."TRESERVEDNAME" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "szName" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "szName")
);

CREATE TABLE "legacy_global"."TSECURECODE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "strSecurityCode" text,
    "bEnabled" integer,
    "bTries" integer,
    "iLockTick" integer,
    "dwUserID" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TSERVER" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "bServerID" smallint NOT NULL CHECK ("bServerID" BETWEEN 0 AND 255),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bMachineID" smallint NOT NULL CHECK ("bMachineID" BETWEEN 0 AND 255),
    "wPort" smallint NOT NULL,
    "szName" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TSMSTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "bWorld" smallint NOT NULL CHECK ("bWorld" BETWEEN 0 AND 255),
    "dwUserID" integer NOT NULL,
    "szCharName" text NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "szSender" text NOT NULL,
    "szMessage" text NOT NULL,
    "dateSend" timestamp without time zone NOT NULL,
    "dwSMS" integer,
    "bResult" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TSVRTYPE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "bControl" smallint NOT NULL CHECK ("bControl" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bType")
);

CREATE TABLE "legacy_global"."TTEMPCASHITEM" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer,
    "dwUserID" integer,
    "wCashItemID" smallint,
    "bCount" smallint CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TTEMPDURINGITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwRemainTime" integer NOT NULL,
    "dEndTime" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "wItemID")
);

CREATE TABLE "legacy_global"."TTESTLOGINUSER" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwuserid" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TUNIFYCASHITEM" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."TUNIFYPOSTITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "dwPostID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "dwPostID")
);

CREATE TABLE "legacy_global"."TUSERATTENDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "bDay" smallint NOT NULL CHECK ("bDay" BETWEEN 0 AND 255),
    "bApply" smallint NOT NULL CHECK ("bApply" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "bDay")
);

CREATE TABLE "legacy_global"."TUSERINFOTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "bCanCreateCharCount" smallint NOT NULL CHECK ("bCanCreateCharCount" BETWEEN 0 AND 255),
    "bAgreement" smallint NOT NULL CHECK ("bAgreement" BETWEEN 0 AND 255),
    "dCabinetUse" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID")
);

CREATE TABLE "legacy_global"."TUSERPROTECTED" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "bBlockType" smallint NOT NULL CHECK ("bBlockType" BETWEEN 0 AND 255),
    "bEternal" smallint NOT NULL CHECK ("bEternal" BETWEEN 0 AND 255),
    "bWorld" smallint CHECK ("bWorld" BETWEEN 0 AND 255),
    "dwCharID" integer,
    "szCharName" text,
    "startTime" timestamp without time zone NOT NULL,
    "dwDuration" integer NOT NULL,
    "bBlockReason" smallint NOT NULL CHECK ("bBlockReason" BETWEEN 0 AND 255),
    "szComment" text NOT NULL,
    "szGMID" text NOT NULL,
    "regDate" timestamp without time zone NOT NULL,
    "sentBanMail" smallint NOT NULL CHECK ("sentBanMail" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwSeq")
);

CREATE TABLE "legacy_global"."TUSERPROTECTED_old" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "bBlockType" smallint NOT NULL CHECK ("bBlockType" BETWEEN 0 AND 255),
    "bEternal" smallint NOT NULL CHECK ("bEternal" BETWEEN 0 AND 255),
    "bWorld" smallint CHECK ("bWorld" BETWEEN 0 AND 255),
    "dwCharID" integer,
    "szCharName" text,
    "startTime" timestamp without time zone NOT NULL,
    "dwDuration" integer NOT NULL,
    "bBlockReason" smallint NOT NULL CHECK ("bBlockReason" BETWEEN 0 AND 255),
    "szComment" text NOT NULL,
    "szGMID" text NOT NULL,
    "regDate" timestamp without time zone NOT NULL,
    "bPerm" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwSeq")
);

CREATE TABLE "legacy_global"."TUSER_INTERFACE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bOption" smallint NOT NULL CHECK ("bOption" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "dwSize" double precision NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bOption")
);

CREATE TABLE "legacy_global"."TVERSION" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwVersion" integer NOT NULL,
    "szPath" text NOT NULL,
    "szName" text NOT NULL,
    "dwSize" integer NOT NULL,
    "dwBetaVer" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwVersion")
);

CREATE TABLE "legacy_global"."TVETERANCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bID" smallint NOT NULL CHECK ("bID" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bID")
);

CREATE TABLE "legacy_global"."TempEvent" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer,
    "CheckDate" timestamp without time zone,
    "dwPlayTime" integer,
    "timeStart" timestamp without time zone,
    "timeEnd" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."USERIPLOG" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "IP" text,
    "Username" text,
    "Date_time" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."releaseDate" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dReleaseDate" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."ttemplevel" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwuserid" integer NOT NULL,
    "bmaxlevel" smallint CHECK ("bmaxlevel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_global"."ttempuser" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bmaxlevel" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."ACCESSORYMAGICTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "ID" integer NOT NULL,
    "MagicID" integer NOT NULL,
    "MinValue" integer NOT NULL,
    "MaxValue" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "ID")
);

CREATE TABLE "legacy_game"."OTESTER" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."STATISTICACCOUNT" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "nAccount" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."STATISTICTEMP" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "nValue1" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TACTIVECHARTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dateEnter" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TAICHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bAIType" smallint NOT NULL CHECK ("bAIType" BETWEEN 0 AND 255),
    "dwCmdID" integer NOT NULL,
    "bTriggerType" smallint NOT NULL CHECK ("bTriggerType" BETWEEN 0 AND 255),
    "dwTriggerID" integer NOT NULL,
    "dwDelay" integer NOT NULL,
    "bLoop" smallint NOT NULL CHECK ("bLoop" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bAIType", "dwCmdID", "bTriggerType", "dwTriggerID")
);

CREATE TABLE "legacy_game"."TAICMDCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCmdID" integer NOT NULL,
    "bCmdType" smallint NOT NULL CHECK ("bCmdType" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCmdID")
);

CREATE TABLE "legacy_game"."TAICONCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCmdID" integer NOT NULL,
    "bConditionType" smallint NOT NULL CHECK ("bConditionType" BETWEEN 0 AND 255),
    "dwConditionID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCmdID", "bConditionType")
);

CREATE TABLE "legacy_game"."TAIDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "dDate" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TARENACHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwFee" integer NOT NULL,
    "wInPos" smallint NOT NULL,
    "wOutPos" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TAUCTIONBIDDER" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwAuctionID" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "dlBidPrice" bigint NOT NULL,
    "DateBid" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwAuctionID", "dwCharID")
);

CREATE TABLE "legacy_game"."TAUCTIONINTEREST" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwAuctionID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "dwAuctionID")
);

CREATE TABLE "legacy_game"."TAUCTIONTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwAuctionID" integer NOT NULL,
    "wNpcID" smallint NOT NULL,
    "dwCharID" integer NOT NULL,
    "DateStart" timestamp without time zone NOT NULL,
    "DateEnd" timestamp without time zone NOT NULL,
    "dlDirectPrice" bigint NOT NULL,
    "dlStartPrice" bigint NOT NULL,
    "dlItemID" bigint NOT NULL,
    "bBidCount" smallint NOT NULL CHECK ("bBidCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwAuctionID")
);

CREATE TABLE "legacy_game"."TBATTLERANKCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bRank" smallint NOT NULL CHECK ("bRank" BETWEEN 0 AND 255),
    "dwPoint" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TBATTLETIMECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwBattleDur" integer NOT NULL,
    "dwBattleStart" integer NOT NULL,
    "dwAlarmStart" integer NOT NULL,
    "dwAlarmEnd" integer NOT NULL,
    "dwPeaceDur" integer NOT NULL,
    "bDay" smallint NOT NULL CHECK ("bDay" BETWEEN 0 AND 255),
    "bWeek" smallint NOT NULL CHECK ("bWeek" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bType")
);

CREATE TABLE "legacy_game"."TBATTLEZONECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "szName" text NOT NULL,
    "wMapID" smallint NOT NULL,
    "wCastle" smallint NOT NULL,
    "wBossSpawnID" smallint NOT NULL,
    "wLGateKeeperSpawnID" smallint NOT NULL,
    "wRGateKeeperSpawnID" smallint NOT NULL,
    "dwLSwitchID" integer NOT NULL,
    "dwRSwitchID" integer NOT NULL,
    "wNormalItem" smallint NOT NULL,
    "wChiefItem" smallint NOT NULL,
    "bLine" smallint NOT NULL CHECK ("bLine" BETWEEN 0 AND 255),
    "wCGateKeeperSpawnID" smallint NOT NULL,
    "dwCSwitchID" integer NOT NULL,
    "wSkill1" smallint NOT NULL,
    "wSkill2" smallint NOT NULL,
    "bItemLevel" smallint NOT NULL CHECK ("bItemLevel" BETWEEN 0 AND 255),
    "wValorianBossDefend" smallint NOT NULL,
    "wValorianBossAttack" smallint NOT NULL,
    "wDerionBossDefend" smallint NOT NULL,
    "wDerionBossAttack" smallint NOT NULL,
    "wMiddleSpawnID" smallint NOT NULL,
    "wRightSpawnID" smallint NOT NULL,
    "wLeftSpawnID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TBOWBONUSITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bItemID" smallint NOT NULL CHECK ("bItemID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "wPrice" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TBOWITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "wMinItemID" smallint NOT NULL,
    "wMaxItemID" smallint NOT NULL,
    "wDependOnItemID" smallint NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TBOWITEMCHART1" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "wMinItemID" smallint NOT NULL,
    "wMaxItemID" smallint NOT NULL,
    "wDependOnItemID" smallint NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TBOWSETTINGSCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wMapID" smallint NOT NULL,
    "bMinPlayersCount" smallint NOT NULL CHECK ("bMinPlayersCount" BETWEEN 0 AND 255),
    "bMaxNationDifference" smallint NOT NULL CHECK ("bMaxNationDifference" BETWEEN 0 AND 255),
    "dwAlarmDur" integer NOT NULL,
    "dwBuyTimeDur" integer NOT NULL,
    "dwBattleDur" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TBRPLAYERTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TBRSETTINGSCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMinPlayerCount" smallint NOT NULL,
    "dwAlarmDur" integer NOT NULL,
    "dwBuyTimeDur" integer NOT NULL,
    "dwBattleDur" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TBRSPAWNPOSCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wMapID" smallint NOT NULL,
    "fPosX" double precision NOT NULL,
    "fPosY" double precision NOT NULL,
    "fPosZ" double precision NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TBRSUPPLIESCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwTick" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCABINETITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bCabinetID" smallint NOT NULL CHECK ("bCabinetID" BETWEEN 0 AND 255),
    "dwStItemID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bCabinetID", "dwStItemID")
);

CREATE TABLE "legacy_game"."TCABINETTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bCabinetID" smallint NOT NULL CHECK ("bCabinetID" BETWEEN 0 AND 255),
    "bUse" smallint NOT NULL CHECK ("bUse" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bCabinetID")
);

CREATE TABLE "legacy_game"."TCASTLEAPPLICANTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wCastleID" smallint NOT NULL,
    "dwCharID" integer NOT NULL,
    "bCamp" smallint NOT NULL CHECK ("bCamp" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TCASTLETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wCastle" smallint NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "dwGuildID" integer NOT NULL,
    "dateWarTime" timestamp without time zone NOT NULL,
    "szHero" text NOT NULL,
    "dateHero" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCHANGEDITEM" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wOld" smallint NOT NULL,
    "wNew" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCHANNELCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "wMapID" smallint NOT NULL,
    "wUnitID" smallint NOT NULL,
    "bLogChannel" smallint NOT NULL CHECK ("bLogChannel" BETWEEN 0 AND 255),
    "bPhyChannel" smallint NOT NULL CHECK ("bPhyChannel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCHARTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "bSlot" smallint NOT NULL CHECK ("bSlot" BETWEEN 0 AND 255),
    "szNAME" text NOT NULL,
    "bStartAct" smallint NOT NULL CHECK ("bStartAct" BETWEEN 0 AND 255),
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
    "bHelmetHide" smallint NOT NULL CHECK ("bHelmetHide" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwEXP" integer NOT NULL,
    "dwHP" integer NOT NULL,
    "dwMP" integer NOT NULL,
    "wSkillPoint" smallint NOT NULL,
    "dwRegion" integer NOT NULL,
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "bGuildLeave" smallint NOT NULL CHECK ("bGuildLeave" BETWEEN 0 AND 255),
    "dwGuildLeaveTime" integer NOT NULL,
    "wMapID" smallint NOT NULL,
    "wSpawnID" smallint NOT NULL,
    "wLastSpawnID" smallint NOT NULL,
    "wTemptedMon" smallint NOT NULL,
    "bAftermath" smallint NOT NULL CHECK ("bAftermath" BETWEEN 0 AND 255),
    "fPosX" real NOT NULL,
    "fPosY" real NOT NULL,
    "fPosZ" real NOT NULL,
    "wDIR" smallint NOT NULL,
    "dwRankPoint" integer NOT NULL,
    "bDelete" smallint NOT NULL CHECK ("bDelete" BETWEEN 0 AND 255),
    "dCreateDate" timestamp without time zone NOT NULL,
    "dDeleteDate" timestamp without time zone,
    "dwLastDestination" integer NOT NULL,
    "bOriCountry" smallint NOT NULL CHECK ("bOriCountry" BETWEEN 0 AND 255),
    "dLogoutDate" timestamp without time zone NOT NULL,
    "bStatLevel" smallint CHECK ("bStatLevel" BETWEEN 0 AND 255),
    "bStatPoint" smallint CHECK ("bStatPoint" BETWEEN 0 AND 255),
    "dwStatExp" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TCLASSCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bClassID" smallint NOT NULL CHECK ("bClassID" BETWEEN 0 AND 255),
    "wSTR" smallint NOT NULL,
    "wDEX" smallint NOT NULL,
    "wCON" smallint NOT NULL,
    "wINT" smallint NOT NULL,
    "wWIS" smallint NOT NULL,
    "wMEN" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bClassID")
);

CREATE TABLE "legacy_game"."TCMGIFTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wGiftID" smallint NOT NULL,
    "bGiftType" smallint NOT NULL CHECK ("bGiftType" BETWEEN 0 AND 255),
    "dwValue" integer NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bTakeType" smallint NOT NULL CHECK ("bTakeType" BETWEEN 0 AND 255),
    "bMaxTakeCount" smallint NOT NULL CHECK ("bMaxTakeCount" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bToolOnly" smallint NOT NULL CHECK ("bToolOnly" BETWEEN 0 AND 255),
    "wErrGiftID" smallint NOT NULL,
    "szTitle" text NOT NULL,
    "szMsg" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wGiftID")
);

CREATE TABLE "legacy_game"."TCMGIFTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "wGiftID" smallint NOT NULL,
    "dwGMCharID" integer NOT NULL,
    "wErrID" smallint NOT NULL,
    "tTakeDate" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCOMPANIONBONUSCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bBonusID" smallint NOT NULL CHECK ("bBonusID" BETWEEN 0 AND 255),
    "fBase" real NOT NULL,
    "fLevelMultiplier" real NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCOMPANIONBONUSCHART_EMPTY" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bBonusID" smallint NOT NULL CHECK ("bBonusID" BETWEEN 0 AND 255),
    "fBase" double precision NOT NULL,
    "fLevelMultiplier" double precision NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCOMPANIONITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bSlot" smallint NOT NULL CHECK ("bSlot" BETWEEN 0 AND 255),
    "wFirstItemID" smallint NOT NULL,
    "wSecondItemID" smallint NOT NULL,
    "dFirstEndTime" timestamp without time zone NOT NULL,
    "dSecondEndTime" timestamp without time zone NOT NULL,
    "dwTick" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCOMPANIONTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bSlot" smallint NOT NULL CHECK ("bSlot" BETWEEN 0 AND 255),
    "dwMonID" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "strName" text NOT NULL,
    "dwExp" integer NOT NULL,
    "wLife" smallint NOT NULL,
    "bStatusPoints" smallint NOT NULL CHECK ("bStatusPoints" BETWEEN 0 AND 255),
    "bEffect" smallint NOT NULL CHECK ("bEffect" BETWEEN 0 AND 255),
    "wSTR" smallint NOT NULL,
    "wDEX" smallint NOT NULL,
    "wCON" smallint NOT NULL,
    "wINT" smallint NOT NULL,
    "wWIS" smallint NOT NULL,
    "wMEN" smallint NOT NULL,
    "wBonusID" smallint NOT NULL,
    "dwCharID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCUSTOMCLOAKTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint,
    "dDateCreate" timestamp without time zone,
    "dwUserID" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TCUSTOMTIMECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint NOT NULL,
    "dwTime" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TDBITEMINDEXTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bWorld" smallint NOT NULL CHECK ("bWorld" BETWEEN 0 AND 255),
    "dlID" bigint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TDESTINATIONCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wPortalID" smallint NOT NULL,
    "wDestID" smallint NOT NULL,
    "dwPrice" integer NOT NULL,
    "bEnable" smallint NOT NULL CHECK ("bEnable" BETWEEN 0 AND 255),
    "bCondition1" smallint NOT NULL CHECK ("bCondition1" BETWEEN 0 AND 255),
    "dwConditionID1" integer NOT NULL,
    "bCondition2" smallint NOT NULL CHECK ("bCondition2" BETWEEN 0 AND 255),
    "dwConditionID2" integer NOT NULL,
    "bCondition3" smallint NOT NULL CHECK ("bCondition3" BETWEEN 0 AND 255),
    "dwConditionID3" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wPortalID", "wDestID")
);

CREATE TABLE "legacy_game"."TDUELCHARTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "szName" text NOT NULL,
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bWin" smallint NOT NULL CHECK ("bWin" BETWEEN 0 AND 255),
    "dwPoint" integer NOT NULL,
    "dTime" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TDUELSCORETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwWarriorWin" integer NOT NULL,
    "dwWarriorLose" integer NOT NULL,
    "dwRangerWin" integer NOT NULL,
    "dwRangerLose" integer NOT NULL,
    "dwArcherWin" integer NOT NULL,
    "dwArcherLose" integer NOT NULL,
    "dwWizardWin" integer NOT NULL,
    "dwWizardLose" integer NOT NULL,
    "dwPriestWin" integer NOT NULL,
    "dwPriestLose" integer NOT NULL,
    "dwSorcererWin" integer NOT NULL,
    "dwSorcererLose" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TEQUIPCREATECHARCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bSex" smallint NOT NULL CHECK ("bSex" BETWEEN 0 AND 255),
    "wPrmWeapon" smallint NOT NULL,
    "wSndWeapon" smallint NOT NULL,
    "wLongWeapon" smallint NOT NULL,
    "wHead" smallint NOT NULL,
    "wBody" smallint NOT NULL,
    "wPants" smallint NOT NULL,
    "wHand" smallint NOT NULL,
    "wFoot" smallint NOT NULL,
    "wBack" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bCountry", "bClass", "bSex")
);

CREATE TABLE "legacy_game"."TERASECHARLOG" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dEraseDate" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TERASEITEM" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TEST" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "DATA" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TEVENTITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bGiveItemCount" smallint NOT NULL CHECK ("bGiveItemCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TEVENTQUARTERCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bDay" smallint NOT NULL CHECK ("bDay" BETWEEN 0 AND 255),
    "bHour" smallint NOT NULL CHECK ("bHour" BETWEEN 0 AND 255),
    "bMinute" smallint NOT NULL CHECK ("bMinute" BETWEEN 0 AND 255),
    "wID" smallint NOT NULL,
    "wItemID1" smallint NOT NULL,
    "wItemID2" smallint NOT NULL,
    "wItemID3" smallint NOT NULL,
    "wItemID4" smallint NOT NULL,
    "wItemID5" smallint NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "szTitle" text NOT NULL,
    "szMessage" text NOT NULL,
    "szPresent" text NOT NULL,
    "szAnnounce" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TEVENTQUARTERGIVETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bDay" smallint NOT NULL CHECK ("bDay" BETWEEN 0 AND 255),
    "bHour" smallint NOT NULL CHECK ("bHour" BETWEEN 0 AND 255),
    "bMinute" smallint NOT NULL CHECK ("bMinute" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "dGiveDate" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TEXPITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwRemainTime" integer NOT NULL,
    "dEndTime" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TFAKECHARCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "dwUserID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TFAKESHOPCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wIndex" smallint NOT NULL,
    "wItemID" smallint NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wIndex", "bCountry")
);

CREATE TABLE "legacy_game"."TFORMULACHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bID" smallint NOT NULL CHECK ("bID" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "dwinit" integer NOT NULL,
    "fRateX" double precision NOT NULL,
    "fRateY" double precision NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bID")
);

CREATE TABLE "legacy_game"."TFRIENDGROUPTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bGroup" smallint NOT NULL CHECK ("bGroup" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bGroup")
);

CREATE TABLE "legacy_game"."TFRIENDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwFriendID" integer NOT NULL,
    "bGroup" smallint NOT NULL CHECK ("bGroup" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "dwFriendID")
);

CREATE TABLE "legacy_game"."TGAMBLECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    "wReplaceID" smallint NOT NULL,
    "bCountMax" smallint NOT NULL CHECK ("bCountMax" BETWEEN 0 AND 255),
    "bMinLevel" smallint NOT NULL CHECK ("bMinLevel" BETWEEN 0 AND 255),
    "bMaxLevel" smallint NOT NULL CHECK ("bMaxLevel" BETWEEN 0 AND 255),
    "wProb" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGATECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwGateID" integer NOT NULL,
    "dwSwitchID" integer NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "wMapID" smallint NOT NULL,
    "wPosX" smallint NOT NULL,
    "wPosY" smallint NOT NULL,
    "wPosZ" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGEMGRADECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bGem" smallint NOT NULL CHECK ("bGem" BETWEEN 0 AND 255),
    "bProb" smallint NOT NULL CHECK ("bProb" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bGem")
);

CREATE TABLE "legacy_game"."TGMREWARDCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dlMoney" bigint NOT NULL,
    "bChartType" smallint NOT NULL CHECK ("bChartType" BETWEEN 0 AND 255),
    "wID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGMREWARDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwCharID" integer NOT NULL,
    "dDate" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "bLevel")
);

CREATE TABLE "legacy_game"."TGODBALLCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bCamp" smallint NOT NULL CHECK ("bCamp" BETWEEN 0 AND 255),
    "wMapID" smallint NOT NULL,
    "fPosX" double precision NOT NULL,
    "fPosY" double precision NOT NULL,
    "fPosZ" double precision NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TGODTOWERCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "wMapID" smallint NOT NULL,
    "fPosX" double precision NOT NULL,
    "fPosY" double precision NOT NULL,
    "fPosZ" double precision NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TGUILDARTICLETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwGuildID" integer NOT NULL,
    "dwID" integer NOT NULL,
    "bDuty" smallint NOT NULL CHECK ("bDuty" BETWEEN 0 AND 255),
    "szWritter" text NOT NULL,
    "szTitle" text NOT NULL,
    "szArticle" text NOT NULL,
    "dwTime" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwGuildID", "dwID")
);

CREATE TABLE "legacy_game"."TGUILDCABINETTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwGuildID" integer NOT NULL,
    "dwItemID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
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
    "dwTime5" integer NOT NULL,
    "dwTime4" integer NOT NULL,
    "dwTime6" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwGuildID", "dwItemID")
);

CREATE TABLE "legacy_game"."TGUILDCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwEXP" integer NOT NULL,
    "bMaxCnt" smallint NOT NULL CHECK ("bMaxCnt" BETWEEN 0 AND 255),
    "bMinCnt" smallint NOT NULL CHECK ("bMinCnt" BETWEEN 0 AND 255),
    "bCabinetCnt" smallint NOT NULL CHECK ("bCabinetCnt" BETWEEN 0 AND 255),
    "bTacticsCnt" smallint NOT NULL CHECK ("bTacticsCnt" BETWEEN 0 AND 255),
    "bBattleSetCnt" smallint NOT NULL CHECK ("bBattleSetCnt" BETWEEN 0 AND 255),
    "bGuardCnt" smallint NOT NULL CHECK ("bGuardCnt" BETWEEN 0 AND 255),
    "bRoyalGuardCnt" smallint NOT NULL CHECK ("bRoyalGuardCnt" BETWEEN 0 AND 255),
    "bTurretCnt" smallint NOT NULL CHECK ("bTurretCnt" BETWEEN 0 AND 255),
    "bPeer1" smallint NOT NULL CHECK ("bPeer1" BETWEEN 0 AND 255),
    "bPeer2" smallint NOT NULL CHECK ("bPeer2" BETWEEN 0 AND 255),
    "bPeer3" smallint NOT NULL CHECK ("bPeer3" BETWEEN 0 AND 255),
    "bPeer4" smallint NOT NULL CHECK ("bPeer4" BETWEEN 0 AND 255),
    "bPeer5" smallint NOT NULL CHECK ("bPeer5" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bLevel")
);

CREATE TABLE "legacy_game"."TGUILDMEMBERSKILLTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wSkillID" smallint,
    "bLevel" smallint CHECK ("bLevel" BETWEEN 0 AND 255),
    "tEndTime" timestamp without time zone,
    "dwCharID" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGUILDMEMBERTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwGuildID" integer NOT NULL,
    "bDuty" smallint NOT NULL CHECK ("bDuty" BETWEEN 0 AND 255),
    "bPeer" smallint NOT NULL CHECK ("bPeer" BETWEEN 0 AND 255),
    "dwService" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TGUILDPLAYLOG" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwGuildID" integer NOT NULL,
    "dwUserID" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "dwPlayTime" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGUILDPVPOINTREWARDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwGuildID" integer NOT NULL,
    "szName" text NOT NULL,
    "dwPoint" integer NOT NULL,
    "dlDate" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGUILDPVPRECORDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwGuildID" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "dwDate" integer NOT NULL,
    "wKillCount" smallint NOT NULL,
    "wDieCount" smallint NOT NULL,
    "dwPoint_1" integer NOT NULL,
    "dwPoint_2" integer NOT NULL,
    "dwPoint_3" integer NOT NULL,
    "dwPoint_4" integer NOT NULL,
    "dwPoint_5" integer NOT NULL,
    "dwPoint_6" integer NOT NULL,
    "dwPoint_7" integer NOT NULL,
    "dwPoint_8" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGUILDRELATION" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwGuildOne" integer NOT NULL,
    "dwGuildTwo" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGUILDSKILLCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint CHECK ("bType" BETWEEN 0 AND 255),
    "wSkillID" smallint,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGUILDSTATSTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwGuildID" integer NOT NULL,
    "bSkillPoint" smallint NOT NULL,
    "bLevel" smallint NOT NULL,
    "dwExp" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TGUILDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "szName" text NOT NULL,
    "dwChief" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwFame" integer NOT NULL,
    "dwFameColor" integer NOT NULL,
    "bMaxCabinet" smallint NOT NULL CHECK ("bMaxCabinet" BETWEEN 0 AND 255),
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "dwGI" integer NOT NULL,
    "dwExp" integer NOT NULL,
    "bGPoint" smallint NOT NULL CHECK ("bGPoint" BETWEEN 0 AND 255),
    "bStatus" smallint NOT NULL CHECK ("bStatus" BETWEEN 0 AND 255),
    "bDisorg" smallint NOT NULL CHECK ("bDisorg" BETWEEN 0 AND 255),
    "dwTime" integer NOT NULL,
    "timeEstablish" timestamp without time zone NOT NULL,
    "dwPvPTotalPoint" integer NOT NULL,
    "dwPvPUseablePoint" integer NOT NULL,
    "dwPvPMonthPoint" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TGUILDTABLE_copy" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "szName" text NOT NULL,
    "dwChief" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwFame" integer NOT NULL,
    "dwFameColor" integer NOT NULL,
    "bMaxCabinet" smallint NOT NULL CHECK ("bMaxCabinet" BETWEEN 0 AND 255),
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "dwGI" integer NOT NULL,
    "dwExp" integer NOT NULL,
    "bGPoint" smallint NOT NULL CHECK ("bGPoint" BETWEEN 0 AND 255),
    "bStatus" smallint NOT NULL CHECK ("bStatus" BETWEEN 0 AND 255),
    "bDisorg" smallint NOT NULL CHECK ("bDisorg" BETWEEN 0 AND 255),
    "dwTime" integer NOT NULL,
    "timeEstablish" timestamp without time zone NOT NULL,
    "dwPvPTotalPoint" integer NOT NULL,
    "dwPvPUseablePoint" integer NOT NULL,
    "dwPvPMonthPoint" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TGUILDTACTICSTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwGuildID" integer NOT NULL,
    "dwRewardPoint" integer NOT NULL,
    "dwGainPoint" integer NOT NULL,
    "bDay" smallint NOT NULL CHECK ("bDay" BETWEEN 0 AND 255),
    "dEndTime" timestamp without time zone NOT NULL,
    "dlRewardMoney" bigint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "dwGuildID")
);

CREATE TABLE "legacy_game"."TGUILDTACTICSWANTEDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "dwGuildID" integer NOT NULL,
    "bMaxLevel" smallint NOT NULL CHECK ("bMaxLevel" BETWEEN 0 AND 255),
    "bMinLevel" smallint NOT NULL CHECK ("bMinLevel" BETWEEN 0 AND 255),
    "dEndTime" timestamp without time zone NOT NULL,
    "szTitle" text NOT NULL,
    "szText" text NOT NULL,
    "bDay" smallint NOT NULL CHECK ("bDay" BETWEEN 0 AND 255),
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "dwPvPoint" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TGUILDVOLUNTEERTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bType")
);

CREATE TABLE "legacy_game"."TGUILDWANTEDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwGuildID" integer NOT NULL,
    "bMaxLevel" smallint NOT NULL CHECK ("bMaxLevel" BETWEEN 0 AND 255),
    "bMinLevel" smallint NOT NULL CHECK ("bMinLevel" BETWEEN 0 AND 255),
    "dEndTime" timestamp without time zone NOT NULL,
    "szTitle" text NOT NULL,
    "szText" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwGuildID")
);

CREATE TABLE "legacy_game"."THELPMESSAGETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bID" smallint NOT NULL CHECK ("bID" BETWEEN 0 AND 255),
    "dStart" timestamp without time zone NOT NULL,
    "dEnd" timestamp without time zone NOT NULL,
    "szMessage" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bID")
);

CREATE TABLE "legacy_game"."THEROTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMonth" smallint NOT NULL CHECK ("bMonth" BETWEEN 0 AND 255),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bOrder" smallint NOT NULL CHECK ("bOrder" BETWEEN 0 AND 255),
    "dwMonthRank" smallint NOT NULL CHECK ("dwMonthRank" BETWEEN 0 AND 255),
    "dwTotalRank" integer NOT NULL,
    "dwTotalPoint" integer NOT NULL,
    "dwMonthPoint" integer NOT NULL,
    "wMonthWin" smallint NOT NULL,
    "wMonthLose" smallint NOT NULL,
    "dwTotalWin" integer NOT NULL,
    "dwTotalLose" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "szName" text NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bRace" text NOT NULL,
    "bSex" smallint NOT NULL CHECK ("bSex" BETWEEN 0 AND 255),
    "bHair" smallint NOT NULL CHECK ("bHair" BETWEEN 0 AND 255),
    "bFace" smallint NOT NULL CHECK ("bFace" BETWEEN 0 AND 255),
    "szGuild" text NOT NULL,
    "szSay" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."THOTKEYTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TINDUNCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wMapID" smallint NOT NULL,
    "wInSpawn" smallint NOT NULL,
    "wOutSpawn_D" smallint NOT NULL,
    "wOutSpawn_C" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wMapID")
);

CREATE TABLE "legacy_game"."TINVENTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "dEndTime" timestamp without time zone NOT NULL,
    "bELD" smallint NOT NULL CHECK ("bELD" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bInvenID")
);

CREATE TABLE "legacy_game"."TINVENTOURNAMENTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL CHECK ("wItemID" BETWEEN 0 AND 255),
    "bELD" smallint NOT NULL CHECK ("bELD" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TITEMATTRCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    "bGrade" smallint NOT NULL CHECK ("bGrade" BETWEEN 0 AND 255),
    "wMinAP" smallint NOT NULL,
    "wMaxAP" smallint NOT NULL,
    "wDP" smallint NOT NULL,
    "wMinMAP" smallint NOT NULL,
    "wMaxMAP" smallint NOT NULL,
    "wMDP" smallint NOT NULL,
    "bBlockProb" smallint NOT NULL CHECK ("bBlockProb" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TITEMCHANGE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bStorageType" smallint NOT NULL CHECK ("bStorageType" BETWEEN 0 AND 255),
    "dwStorageID" integer NOT NULL,
    "bOwnerType" smallint NOT NULL CHECK ("bOwnerType" BETWEEN 0 AND 255),
    "dwOwnerID" integer NOT NULL,
    "bItemID" smallint NOT NULL CHECK ("bItemID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
    "dwDuraCur" integer NOT NULL,
    "bRefineCur" smallint NOT NULL CHECK ("bRefineCur" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wItemID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    "szNAME" text NOT NULL,
    "wAttrID" smallint NOT NULL,
    "wUseValue" smallint NOT NULL,
    "dwSlotID" integer NOT NULL,
    "dwClassID" integer NOT NULL,
    "bPrmSlotID" smallint NOT NULL CHECK ("bPrmSlotID" BETWEEN 0 AND 255),
    "bSubSlotID" smallint NOT NULL CHECK ("bSubSlotID" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "fPrice" double precision NOT NULL,
    "bIsSell" smallint NOT NULL CHECK ("bIsSell" BETWEEN 0 AND 255),
    "bMinRange" smallint NOT NULL CHECK ("bMinRange" BETWEEN 0 AND 255),
    "bMaxRange" smallint NOT NULL CHECK ("bMaxRange" BETWEEN 0 AND 255),
    "bStack" smallint NOT NULL CHECK ("bStack" BETWEEN 0 AND 255),
    "bEquipSkill" smallint NOT NULL CHECK ("bEquipSkill" BETWEEN 0 AND 255),
    "bSlotCount" smallint NOT NULL CHECK ("bSlotCount" BETWEEN 0 AND 255),
    "bUseItemKind" smallint NOT NULL CHECK ("bUseItemKind" BETWEEN 0 AND 255),
    "bUseItemCount" smallint NOT NULL CHECK ("bUseItemCount" BETWEEN 0 AND 255),
    "bGrade" smallint NOT NULL CHECK ("bGrade" BETWEEN 0 AND 255),
    "wUseTime" smallint NOT NULL,
    "bUseType" smallint NOT NULL CHECK ("bUseType" BETWEEN 0 AND 255),
    "bCanGrade" smallint NOT NULL CHECK ("bCanGrade" BETWEEN 0 AND 255),
    "bCanMagic" smallint NOT NULL CHECK ("bCanMagic" BETWEEN 0 AND 255),
    "bCanRare" smallint NOT NULL CHECK ("bCanRare" BETWEEN 0 AND 255),
    "bDropLevel" smallint NOT NULL CHECK ("bDropLevel" BETWEEN 0 AND 255),
    "dwSpeedInc" integer NOT NULL,
    "bItemCountry" smallint NOT NULL CHECK ("bItemCountry" BETWEEN 0 AND 255),
    "bIsSpecial" smallint NOT NULL CHECK ("bIsSpecial" BETWEEN 0 AND 255),
    "dwDelay" integer NOT NULL,
    "fRevision" double precision NOT NULL,
    "fMRevision" double precision NOT NULL,
    "fAtRate" double precision NOT NULL,
    "fMAtRate" double precision NOT NULL,
    "bCanGamble" smallint NOT NULL CHECK ("bCanGamble" BETWEEN 0 AND 255),
    "wItemProb_G" smallint NOT NULL,
    "bDestroyProb" smallint NOT NULL CHECK ("bDestroyProb" BETWEEN 0 AND 255),
    "bGambleProb" smallint NOT NULL CHECK ("bGambleProb" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
    "bRefineMax" smallint NOT NULL CHECK ("bRefineMax" BETWEEN 0 AND 255),
    "bCanRepair" smallint NOT NULL CHECK ("bCanRepair" BETWEEN 0 AND 255),
    "wDelayGroupID" smallint NOT NULL,
    "wWeight" smallint NOT NULL,
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "bInitState" smallint NOT NULL CHECK ("bInitState" BETWEEN 0 AND 255),
    "bCanWrap" smallint NOT NULL CHECK ("bCanWrap" BETWEEN 0 AND 255),
    "dwCode" integer NOT NULL,
    "bCanColor" smallint NOT NULL CHECK ("bCanColor" BETWEEN 0 AND 255),
    "fPvPrice" double precision NOT NULL,
    "bConsumable" smallint NOT NULL CHECK ("bConsumable" BETWEEN 0 AND 255),
    "wExpandValue" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wItemID")
);

CREATE TABLE "legacy_game"."TITEMCHART_bak" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wItemID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    "szNAME" text NOT NULL,
    "wAttrID" smallint NOT NULL,
    "wUseValue" smallint NOT NULL,
    "dwSlotID" integer NOT NULL,
    "dwClassID" integer NOT NULL,
    "bPrmSlotID" smallint NOT NULL CHECK ("bPrmSlotID" BETWEEN 0 AND 255),
    "bSubSlotID" smallint NOT NULL CHECK ("bSubSlotID" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "fPrice" double precision NOT NULL,
    "bIsSell" smallint NOT NULL CHECK ("bIsSell" BETWEEN 0 AND 255),
    "bMinRange" smallint NOT NULL CHECK ("bMinRange" BETWEEN 0 AND 255),
    "bMaxRange" smallint NOT NULL CHECK ("bMaxRange" BETWEEN 0 AND 255),
    "bStack" smallint NOT NULL CHECK ("bStack" BETWEEN 0 AND 255),
    "bEquipSkill" smallint NOT NULL CHECK ("bEquipSkill" BETWEEN 0 AND 255),
    "bSlotCount" smallint NOT NULL CHECK ("bSlotCount" BETWEEN 0 AND 255),
    "bUseItemKind" smallint NOT NULL CHECK ("bUseItemKind" BETWEEN 0 AND 255),
    "bUseItemCount" smallint NOT NULL CHECK ("bUseItemCount" BETWEEN 0 AND 255),
    "bGrade" smallint NOT NULL CHECK ("bGrade" BETWEEN 0 AND 255),
    "wUseTime" smallint NOT NULL,
    "bUseType" smallint NOT NULL CHECK ("bUseType" BETWEEN 0 AND 255),
    "bCanGrade" smallint NOT NULL CHECK ("bCanGrade" BETWEEN 0 AND 255),
    "bCanMagic" smallint NOT NULL CHECK ("bCanMagic" BETWEEN 0 AND 255),
    "bCanRare" smallint NOT NULL CHECK ("bCanRare" BETWEEN 0 AND 255),
    "bDropLevel" smallint NOT NULL CHECK ("bDropLevel" BETWEEN 0 AND 255),
    "dwSpeedInc" integer NOT NULL,
    "bItemCountry" smallint NOT NULL CHECK ("bItemCountry" BETWEEN 0 AND 255),
    "bIsSpecial" smallint NOT NULL CHECK ("bIsSpecial" BETWEEN 0 AND 255),
    "dwDelay" integer NOT NULL,
    "fRevision" double precision NOT NULL,
    "fMRevision" double precision NOT NULL,
    "fAtRate" double precision NOT NULL,
    "fMAtRate" double precision NOT NULL,
    "bCanGamble" smallint NOT NULL CHECK ("bCanGamble" BETWEEN 0 AND 255),
    "wItemProb_G" smallint NOT NULL,
    "bDestroyProb" smallint NOT NULL CHECK ("bDestroyProb" BETWEEN 0 AND 255),
    "bGambleProb" smallint NOT NULL CHECK ("bGambleProb" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
    "bRefineMax" smallint NOT NULL CHECK ("bRefineMax" BETWEEN 0 AND 255),
    "bCanRepair" smallint NOT NULL CHECK ("bCanRepair" BETWEEN 0 AND 255),
    "wDelayGroupID" smallint NOT NULL,
    "wWeight" smallint NOT NULL,
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "bInitState" smallint NOT NULL CHECK ("bInitState" BETWEEN 0 AND 255),
    "bCanWrap" smallint NOT NULL CHECK ("bCanWrap" BETWEEN 0 AND 255),
    "dwCode" integer NOT NULL,
    "bCanColor" smallint NOT NULL CHECK ("bCanColor" BETWEEN 0 AND 255),
    "fPvPrice" double precision NOT NULL,
    "bConsumable" smallint NOT NULL CHECK ("bConsumable" BETWEEN 0 AND 255),
    "wExpandValue" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wItemID")
);

CREATE TABLE "legacy_game"."TITEMGRADECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bGrade" smallint NOT NULL CHECK ("bGrade" BETWEEN 0 AND 255),
    "bProb" smallint NOT NULL CHECK ("bProb" BETWEEN 0 AND 255),
    "dwMoney" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TITEMLEVELCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bGrade" smallint NOT NULL CHECK ("bGrade" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bGrade")
);

CREATE TABLE "legacy_game"."TITEMLOG" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "bItemID" smallint NOT NULL CHECK ("bItemID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bInsType" smallint NOT NULL CHECK ("bInsType" BETWEEN 0 AND 255),
    "dwTargetID" integer NOT NULL,
    "szTargetName" text NOT NULL,
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
    "timeInsert" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TITEMMAGICCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMagic" smallint NOT NULL CHECK ("bMagic" BETWEEN 0 AND 255),
    "dwKind" integer NOT NULL,
    "bRvType" smallint NOT NULL CHECK ("bRvType" BETWEEN 0 AND 255),
    "wMaxValue" smallint NOT NULL,
    "bIsMagic" smallint NOT NULL CHECK ("bIsMagic" BETWEEN 0 AND 255),
    "bIsRare" smallint NOT NULL CHECK ("bIsRare" BETWEEN 0 AND 255),
    "bMinLevel" smallint NOT NULL CHECK ("bMinLevel" BETWEEN 0 AND 255),
    "bExclIndex" smallint NOT NULL CHECK ("bExclIndex" BETWEEN 0 AND 255),
    "bOptionKind" smallint NOT NULL CHECK ("bOptionKind" BETWEEN 0 AND 255),
    "wAutoSkill" smallint NOT NULL,
    "bRefine" smallint NOT NULL CHECK ("bRefine" BETWEEN 0 AND 255),
    "wMaxBound" smallint NOT NULL,
    "wRareBound" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bMagic")
);

CREATE TABLE "legacy_game"."TITEMMAGICLEVELCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSection" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwSection")
);

CREATE TABLE "legacy_game"."TITEMMAGICSKILLCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMagic" smallint NOT NULL CHECK ("bMagic" BETWEEN 0 AND 255),
    "dwKind" integer NOT NULL,
    "wSkillID" smallint NOT NULL,
    "bIsMagic" smallint NOT NULL CHECK ("bIsMagic" BETWEEN 0 AND 255),
    "bIsRare" smallint NOT NULL CHECK ("bIsRare" BETWEEN 0 AND 255),
    "bMinLevel" smallint NOT NULL CHECK ("bMinLevel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TITEMSETCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wBaseID" smallint NOT NULL,
    "wSetID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bMagic1" smallint NOT NULL CHECK ("bMagic1" BETWEEN 0 AND 255),
    "bMin1" smallint NOT NULL CHECK ("bMin1" BETWEEN 0 AND 255),
    "bValue1" smallint NOT NULL CHECK ("bValue1" BETWEEN 0 AND 255),
    "bMagic2" smallint NOT NULL CHECK ("bMagic2" BETWEEN 0 AND 255),
    "bMin2" smallint NOT NULL CHECK ("bMin2" BETWEEN 0 AND 255),
    "bValue2" smallint NOT NULL CHECK ("bValue2" BETWEEN 0 AND 255),
    "bMagic3" smallint NOT NULL CHECK ("bMagic3" BETWEEN 0 AND 255),
    "bMin3" smallint NOT NULL CHECK ("bMin3" BETWEEN 0 AND 255),
    "bValue3" smallint NOT NULL CHECK ("bValue3" BETWEEN 0 AND 255),
    "bMagic4" smallint NOT NULL CHECK ("bMagic4" BETWEEN 0 AND 255),
    "bMin4" smallint NOT NULL CHECK ("bMin4" BETWEEN 0 AND 255),
    "bValue4" smallint NOT NULL CHECK ("bValue4" BETWEEN 0 AND 255),
    "bMagic5" smallint NOT NULL CHECK ("bMagic5" BETWEEN 0 AND 255),
    "bMin5" smallint NOT NULL CHECK ("bMin5" BETWEEN 0 AND 255),
    "bValue5" smallint NOT NULL CHECK ("bValue5" BETWEEN 0 AND 255),
    "bMagic6" smallint NOT NULL CHECK ("bMagic6" BETWEEN 0 AND 255),
    "bMin6" smallint NOT NULL CHECK ("bMin6" BETWEEN 0 AND 255),
    "bValue6" smallint NOT NULL CHECK ("bValue6" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wBaseID", "wSetID")
);

CREATE TABLE "legacy_game"."TITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dlID" bigint NOT NULL,
    "bStorageType" smallint NOT NULL CHECK ("bStorageType" BETWEEN 0 AND 255),
    "dwStorageID" integer NOT NULL,
    "bOwnerType" smallint NOT NULL CHECK ("bOwnerType" BETWEEN 0 AND 255),
    "dwOwnerID" integer NOT NULL,
    "bItemID" smallint NOT NULL CHECK ("bItemID" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dlID")
);

CREATE TABLE "legacy_game"."TITEMTOURNAMENTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "bItemID" smallint NOT NULL CHECK ("bItemID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
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
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TITEMUSEDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wDelayGroupID" smallint NOT NULL,
    "dwTick" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "wDelayGroupID")
);

CREATE TABLE "legacy_game"."TLASTCOMPANIONTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bCompanionSlot" smallint NOT NULL CHECK ("bCompanionSlot" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TLEVELCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwEXP" integer NOT NULL,
    "dwHP" integer NOT NULL,
    "dwMP" integer NOT NULL,
    "bSkillPoint" smallint NOT NULL CHECK ("bSkillPoint" BETWEEN 0 AND 255),
    "dwMoney" integer NOT NULL,
    "dwScore" integer NOT NULL,
    "dwRegCost" integer NOT NULL,
    "dwSearchCost" integer NOT NULL,
    "dwGambleCost" integer NOT NULL,
    "dwRepCost" integer NOT NULL,
    "dwRepairCost" integer NOT NULL,
    "dwRefineCost" integer NOT NULL,
    "wPvPoint" smallint NOT NULL,
    "dwPvPMoney" integer NOT NULL,
    "dwPvPExp" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bLevel")
);

CREATE TABLE "legacy_game"."TLOCALOCCUPYTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wLocalID" smallint NOT NULL,
    "bDay" smallint NOT NULL CHECK ("bDay" BETWEEN 0 AND 255),
    "dwGuildID" integer NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wLocalID", "bDay")
);

CREATE TABLE "legacy_game"."TLOCALTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wLocalID" smallint NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "dwGuild" integer NOT NULL,
    "dateOccupy" timestamp without time zone NOT NULL,
    "dateDefend" timestamp without time zone NOT NULL,
    "szHero" text NOT NULL,
    "dateHero" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wLocalID")
);

CREATE TABLE "legacy_game"."TMAPCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bGroupID" smallint NOT NULL CHECK ("bGroupID" BETWEEN 0 AND 255),
    "wMapID" smallint NOT NULL,
    "bServerID" smallint NOT NULL CHECK ("bServerID" BETWEEN 0 AND 255),
    "bChannel" smallint NOT NULL CHECK ("bChannel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bGroupID", "wMapID", "bServerID")
);

CREATE TABLE "legacy_game"."TMAPMONCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wSpawnID" smallint NOT NULL,
    "wMonID" smallint NOT NULL,
    "bEssential" smallint NOT NULL CHECK ("bEssential" BETWEEN 0 AND 255),
    "bLeader" smallint NOT NULL CHECK ("bLeader" BETWEEN 0 AND 255),
    "bProb" smallint NOT NULL CHECK ("bProb" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wSpawnID", "wMonID", "bEssential")
);

CREATE TABLE "legacy_game"."TMEDALS" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwMedals" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TMENTORTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwMentorID" integer NOT NULL,
    "dwExp" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TMISSIONTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wMissionID" smallint NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TMONATTRCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "wAP" smallint NOT NULL,
    "wLAP" smallint NOT NULL,
    "dwAtkSpeed" integer NOT NULL,
    "wAL" smallint NOT NULL,
    "wDL" smallint NOT NULL,
    "bCriticalPP" smallint NOT NULL CHECK ("bCriticalPP" BETWEEN 0 AND 255),
    "dwMaxHP" integer NOT NULL,
    "bHPRecover" smallint NOT NULL CHECK ("bHPRecover" BETWEEN 0 AND 255),
    "wMAP" smallint NOT NULL,
    "bCriticalMP" smallint NOT NULL,
    "dwMaxMP" integer NOT NULL,
    "bMPRecover" smallint NOT NULL CHECK ("bMPRecover" BETWEEN 0 AND 255),
    "wDP" smallint NOT NULL,
    "wMDP" smallint NOT NULL,
    "wMinWAP" smallint NOT NULL,
    "wMaxWAP" smallint NOT NULL,
    "wWDP" smallint NOT NULL,
    "wMAL" smallint NOT NULL,
    "wMDL" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID", "bLevel")
);

CREATE TABLE "legacy_game"."TMONITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bChartType" smallint NOT NULL CHECK ("bChartType" BETWEEN 0 AND 255),
    "wMonID" smallint NOT NULL,
    "wItemID" smallint NOT NULL,
    "wItemIDMin" smallint NOT NULL,
    "wItemIDMax" smallint NOT NULL,
    "bLevelMin" smallint NOT NULL CHECK ("bLevelMin" BETWEEN 0 AND 255),
    "bLevelMax" smallint NOT NULL CHECK ("bLevelMax" BETWEEN 0 AND 255),
    "bItemProb_N1" smallint NOT NULL CHECK ("bItemProb_N1" BETWEEN 0 AND 255),
    "bItemProb_N2" smallint NOT NULL CHECK ("bItemProb_N2" BETWEEN 0 AND 255),
    "bItemProb_N3" smallint NOT NULL CHECK ("bItemProb_N3" BETWEEN 0 AND 255),
    "bItemProb_N4" smallint NOT NULL CHECK ("bItemProb_N4" BETWEEN 0 AND 255),
    "bItemProb_M" smallint NOT NULL CHECK ("bItemProb_M" BETWEEN 0 AND 255),
    "bItemProb_S" smallint NOT NULL CHECK ("bItemProb_S" BETWEEN 0 AND 255),
    "bItemProb_R" smallint NOT NULL CHECK ("bItemProb_R" BETWEEN 0 AND 255),
    "bItemMagicOpt" smallint NOT NULL CHECK ("bItemMagicOpt" BETWEEN 0 AND 255),
    "bItemRareOpt" smallint NOT NULL CHECK ("bItemRareOpt" BETWEEN 0 AND 255),
    "wWeight" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wMonID", "wItemID", "bChartType")
);

CREATE TABLE "legacy_game"."TMONSPAWNCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "wGroup" smallint NOT NULL,
    "wLocalID" smallint NOT NULL,
    "wMapID" smallint NOT NULL,
    "fPosX" real NOT NULL,
    "fPosY" real NOT NULL,
    "fPosZ" real NOT NULL,
    "wDir" smallint NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bRange" smallint NOT NULL CHECK ("bRange" BETWEEN 0 AND 255),
    "bArea" smallint NOT NULL CHECK ("bArea" BETWEEN 0 AND 255),
    "bLink" smallint NOT NULL CHECK ("bLink" BETWEEN 0 AND 255),
    "bProb" smallint NOT NULL CHECK ("bProb" BETWEEN 0 AND 255),
    "bRoamType" smallint NOT NULL CHECK ("bRoamType" BETWEEN 0 AND 255),
    "dwRegion" integer NOT NULL,
    "dwDelay" integer NOT NULL,
    "bEvent" smallint NOT NULL CHECK ("bEvent" BETWEEN 0 AND 255),
    "wPartyID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TMONSTERCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "szName" text NOT NULL,
    "szName2" text NOT NULL,
    "bRace" smallint NOT NULL CHECK ("bRace" BETWEEN 0 AND 255),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "wKind" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bAIType" smallint NOT NULL CHECK ("bAIType" BETWEEN 0 AND 255),
    "bRange" smallint NOT NULL CHECK ("bRange" BETWEEN 0 AND 255),
    "wChaseRange" smallint NOT NULL,
    "bRoamProb" smallint NOT NULL CHECK ("bRoamProb" BETWEEN 0 AND 255),
    "bMoneyProb" smallint NOT NULL CHECK ("bMoneyProb" BETWEEN 0 AND 255),
    "dwMinMoney" integer NOT NULL,
    "dwMaxMoney" integer NOT NULL,
    "bItemProb" smallint NOT NULL CHECK ("bItemProb" BETWEEN 0 AND 255),
    "bDropCount" smallint NOT NULL CHECK ("bDropCount" BETWEEN 0 AND 255),
    "wExp" smallint NOT NULL,
    "bIsSelf" smallint NOT NULL CHECK ("bIsSelf" BETWEEN 0 AND 255),
    "bRecallType" smallint NOT NULL CHECK ("bRecallType" BETWEEN 0 AND 255),
    "bCanSelect" smallint NOT NULL CHECK ("bCanSelect" BETWEEN 0 AND 255),
    "bCanAttack" smallint NOT NULL CHECK ("bCanAttack" BETWEEN 0 AND 255),
    "bTame" smallint NOT NULL CHECK ("bTame" BETWEEN 0 AND 255),
    "bCall" smallint NOT NULL CHECK ("bCall" BETWEEN 0 AND 255),
    "bIsSpecial" smallint NOT NULL CHECK ("bIsSpecial" BETWEEN 0 AND 255),
    "bRemove" smallint NOT NULL CHECK ("bRemove" BETWEEN 0 AND 255),
    "wMonAttr" smallint NOT NULL,
    "wSummonAttr" smallint NOT NULL,
    "wTransSkillID" smallint NOT NULL,
    "fSize" double precision NOT NULL,
    "wSkill1" smallint NOT NULL,
    "wSkill2" smallint NOT NULL,
    "wSkill3" smallint NOT NULL,
    "wSkill4" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TMONSTERSHOPCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "wNpcID" smallint NOT NULL,
    "wSpawnID" smallint NOT NULL,
    "dwPrice" integer NOT NULL,
    "wTowerID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TMONTHPVPOINTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "dwPoint" integer NOT NULL,
    "wWin" smallint NOT NULL,
    "wLose" smallint NOT NULL,
    "szSay" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bCountry")
);

CREATE TABLE "legacy_game"."TMONTHRANKCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bRank" smallint NOT NULL CHECK ("bRank" BETWEEN 0 AND 255),
    "bChartType1" smallint NOT NULL CHECK ("bChartType1" BETWEEN 0 AND 255),
    "wItemID1" smallint NOT NULL,
    "bCount1" smallint NOT NULL CHECK ("bCount1" BETWEEN 0 AND 255),
    "bChartType2" smallint NOT NULL CHECK ("bChartType2" BETWEEN 0 AND 255),
    "wItemID2" smallint NOT NULL,
    "bCount2" smallint NOT NULL CHECK ("bCount2" BETWEEN 0 AND 255),
    "bChartType3" smallint NOT NULL CHECK ("bChartType3" BETWEEN 0 AND 255),
    "wItemID3" smallint NOT NULL,
    "bCount3" smallint NOT NULL CHECK ("bCount3" BETWEEN 0 AND 255),
    "bChartType4" smallint NOT NULL CHECK ("bChartType4" BETWEEN 0 AND 255),
    "wItemID4" smallint NOT NULL,
    "bCount4" smallint NOT NULL CHECK ("bCount4" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bRank")
);

CREATE TABLE "legacy_game"."TMONTHRANKTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bMonth" smallint NOT NULL CHECK ("bMonth" BETWEEN 0 AND 255),
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bRank" smallint NOT NULL CHECK ("bRank" BETWEEN 0 AND 255),
    "bMonthRank" smallint NOT NULL CHECK ("bMonthRank" BETWEEN 0 AND 255),
    "dwTotalRank" integer NOT NULL,
    "dwCharID" integer NOT NULL,
    "szName" text NOT NULL,
    "dwTotalPoint" integer NOT NULL,
    "dwMonthPoint" integer NOT NULL,
    "wMonthWin" smallint NOT NULL,
    "wMonthLose" smallint NOT NULL,
    "dwTotalWin" integer NOT NULL,
    "dwTotalLose" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bRace" smallint NOT NULL CHECK ("bRace" BETWEEN 0 AND 255),
    "bSex" smallint NOT NULL CHECK ("bSex" BETWEEN 0 AND 255),
    "bHair" smallint NOT NULL CHECK ("bHair" BETWEEN 0 AND 255),
    "bFace" smallint NOT NULL CHECK ("bFace" BETWEEN 0 AND 255),
    "szSay" text NOT NULL,
    "szGuild" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TMOUNTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wMountID" smallint NOT NULL,
    "wDefMonID" smallint NOT NULL,
    "wUpgMonID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wMountID")
);

CREATE TABLE "legacy_game"."TMOUNTITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dEndTime" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "wItemID")
);

CREATE TABLE "legacy_game"."TMOUNTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wMountID" smallint NOT NULL,
    "szName" text NOT NULL,
    "dEndTime" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "wMountID")
);

CREATE TABLE "legacy_game"."TNPCCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "NC_szName2" text NOT NULL,
    "szName" text NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwClass" integer NOT NULL,
    "bCountryID" smallint NOT NULL CHECK ("bCountryID" BETWEEN 0 AND 255),
    "wLocalID" smallint NOT NULL,
    "bCondition" smallint NOT NULL CHECK ("bCondition" BETWEEN 0 AND 255),
    "bDiscountRate" smallint NOT NULL CHECK ("bDiscountRate" BETWEEN 0 AND 255),
    "bAddProb" smallint NOT NULL CHECK ("bAddProb" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "wMapID" smallint NOT NULL,
    "fPosX" double precision NOT NULL,
    "fPosY" double precision NOT NULL,
    "fPosZ" double precision NOT NULL,
    "szLocal" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TNPCITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wNpcID" smallint NOT NULL,
    "dwItemID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wNpcID", "dwItemID")
);

CREATE TABLE "legacy_game"."TOPERATORTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwOperatorID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwOperatorID")
);

CREATE TABLE "legacy_game"."TPETCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bPetType" smallint NOT NULL CHECK ("bPetType" BETWEEN 0 AND 255),
    "bRace" smallint NOT NULL CHECK ("bRace" BETWEEN 0 AND 255),
    "wMonID" smallint NOT NULL,
    "bRecallKind1" smallint NOT NULL CHECK ("bRecallKind1" BETWEEN 0 AND 255),
    "bRecallKind2" smallint NOT NULL CHECK ("bRecallKind2" BETWEEN 0 AND 255),
    "wRecallValue1" smallint NOT NULL,
    "wRecallValue2" smallint NOT NULL,
    "bConditionType" smallint NOT NULL CHECK ("bConditionType" BETWEEN 0 AND 255),
    "dwConditionValue" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TPETTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "wPetID" smallint NOT NULL,
    "szName" text NOT NULL,
    "timeUse" timestamp without time zone NOT NULL,
    "bEffect" smallint CHECK ("bEffect" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "wPetID")
);

CREATE TABLE "legacy_game"."TPLAYTIMETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwPlayTime" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TPORTALCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wPortalID" smallint NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "wLocalID" smallint NOT NULL,
    "wSpawnID" smallint NOT NULL,
    "bCondition" smallint NOT NULL CHECK ("bCondition" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wPortalID")
);

CREATE TABLE "legacy_game"."TPOSTERRORTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwGold" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TPOSTITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwPostID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TPOSTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwPostID" integer NOT NULL,
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
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwPostID")
);

CREATE TABLE "legacy_game"."TPREMIUMSKILLCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "wSkillID" smallint NOT NULL,
    "wMedals" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TPROTECTEDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwProtected" integer NOT NULL,
    "szNAME" text NOT NULL,
    "bOption" smallint NOT NULL CHECK ("bOption" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "dwProtected")
);

CREATE TABLE "legacy_game"."TPVPOINTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wLocalID" smallint NOT NULL,
    "bStatus" smallint NOT NULL CHECK ("bStatus" BETWEEN 0 AND 255),
    "bEvent" smallint NOT NULL CHECK ("bEvent" BETWEEN 0 AND 255),
    "bTarget" smallint NOT NULL CHECK ("bTarget" BETWEEN 0 AND 255),
    "dwIncPoint" integer NOT NULL,
    "dwDecPoint" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TPVPOINTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwUseablePoint" integer NOT NULL,
    "dwTotalPoint" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TPVPRECENTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "szName" text NOT NULL,
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bWin" smallint NOT NULL CHECK ("bWin" BETWEEN 0 AND 255),
    "dwPoint" integer NOT NULL,
    "dlDate" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TPVPRECORDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwWarrior_win" integer NOT NULL,
    "dwWarrior_lose" integer NOT NULL,
    "dwRanger_win" integer NOT NULL,
    "dwRanger_lose" integer NOT NULL,
    "dwArcher_win" integer NOT NULL,
    "dwArcher_lose" integer NOT NULL,
    "dwWizard_win" integer NOT NULL,
    "dwWizard_lose" integer NOT NULL,
    "dwPriest_win" integer NOT NULL,
    "dwPriest_lose" integer NOT NULL,
    "dwSorcerer_win" integer NOT NULL,
    "dwSorcerer_lose" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TQCLASSCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwClassID" integer NOT NULL,
    "szNAME" text NOT NULL,
    "bClassMain" smallint NOT NULL CHECK ("bClassMain" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwClassID")
);

CREATE TABLE "legacy_game"."TQCONDITIONCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "dwQuestID" integer NOT NULL,
    "bConditionType" smallint NOT NULL CHECK ("bConditionType" BETWEEN 0 AND 255),
    "dwConditionID" integer NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TQREWARDCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "dwQuestID" integer NOT NULL,
    "bRewardType" smallint NOT NULL CHECK ("bRewardType" BETWEEN 0 AND 255),
    "dwRewardID" integer NOT NULL,
    "bTakeMethod" smallint NOT NULL CHECK ("bTakeMethod" BETWEEN 0 AND 255),
    "bTakeData" smallint NOT NULL CHECK ("bTakeData" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "dwQuestMob" integer NOT NULL,
    "dwQuestTime" integer NOT NULL,
    "dwQuestPathMob" integer NOT NULL,
    "dwTicketID" integer NOT NULL,
    "bSendQ" smallint NOT NULL CHECK ("bSendQ" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TQTITLECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwQuestID" integer NOT NULL,
    "dwClassID" integer NOT NULL,
    "szTitle" text,
    "szMessage" text,
    "szComplete" text,
    "szAccept" text,
    "szReject" text,
    "szSummary" text,
    "szNPCName" text,
    "szReply" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwQuestID")
);

CREATE TABLE "legacy_game"."TQUESTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwQuestID" integer NOT NULL,
    "dwParentID" integer NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bForceRun" smallint NOT NULL CHECK ("bForceRun" BETWEEN 0 AND 255),
    "bTriggerType" smallint NOT NULL CHECK ("bTriggerType" BETWEEN 0 AND 255),
    "dwTriggerID" integer NOT NULL,
    "bCountMax" smallint NOT NULL CHECK ("bCountMax" BETWEEN 0 AND 255),
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bMain" smallint NOT NULL CHECK ("bMain" BETWEEN 0 AND 255),
    "bConditionCheck" smallint NOT NULL CHECK ("bConditionCheck" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwQuestID")
);

CREATE TABLE "legacy_game"."TQUESTITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
    "bDropLevel" smallint NOT NULL CHECK ("bDropLevel" BETWEEN 0 AND 255),
    "dwDuraMax" integer NOT NULL,
    "dwDuraCur" integer NOT NULL,
    "bRefineCur" smallint NOT NULL CHECK ("bRefineCur" BETWEEN 0 AND 255),
    "wUseTime" smallint NOT NULL,
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
    "dwMoney" integer NOT NULL,
    "bGem" smallint NOT NULL CHECK ("bGem" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TQUESTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwQuestID" integer NOT NULL,
    "dwTick" integer NOT NULL,
    "bCompleteCount" smallint NOT NULL CHECK ("bCompleteCount" BETWEEN 0 AND 255),
    "bTriggerCount" smallint NOT NULL CHECK ("bTriggerCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "dwQuestID")
);

CREATE TABLE "legacy_game"."TQUESTTERMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "dwQuestID" integer NOT NULL,
    "bTermType" smallint NOT NULL CHECK ("bTermType" BETWEEN 0 AND 255),
    "dwTermID" integer NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TQUESTTERMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwQuestID" integer NOT NULL,
    "dwTermID" integer NOT NULL,
    "bTermType" smallint NOT NULL CHECK ("bTermType" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "dwQuestID", "dwTermID")
);

CREATE TABLE "legacy_game"."TRACECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bRaceID" smallint NOT NULL CHECK ("bRaceID" BETWEEN 0 AND 255),
    "wSTR" smallint NOT NULL,
    "wDEX" smallint NOT NULL,
    "wCON" smallint NOT NULL,
    "wINT" smallint NOT NULL,
    "wWIS" smallint NOT NULL,
    "wMEN" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bRaceID")
);

CREATE TABLE "legacy_game"."TRANKING" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwRankPoint" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TRECALLMAINTAINTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwRecallID" integer NOT NULL,
    "wSkillID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwRemainTick" integer NOT NULL,
    "bAttackType" smallint NOT NULL CHECK ("bAttackType" BETWEEN 0 AND 255),
    "dwAttackID" integer NOT NULL,
    "bHostType" text NOT NULL,
    "dwHostID" integer NOT NULL,
    "bAttackCountry" smallint NOT NULL CHECK ("bAttackCountry" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TRECALLMONTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwOwnerID" integer NOT NULL,
    "dwID" integer NOT NULL,
    "wMonID" smallint NOT NULL,
    "wPetID" smallint NOT NULL,
    "dwATTR" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwHP" integer NOT NULL,
    "dwMP" integer NOT NULL,
    "bSkillLevel" smallint NOT NULL CHECK ("bSkillLevel" BETWEEN 0 AND 255),
    "wPosX" smallint NOT NULL,
    "wPosY" smallint NOT NULL,
    "wPosZ" smallint NOT NULL,
    "dwTime" integer NOT NULL,
    "bEffect" smallint NOT NULL CHECK ("bEffect" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TRESERVEDPOST" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSeq" integer NOT NULL,
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
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwSeq")
);

CREATE TABLE "legacy_game"."TRPSGAMECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bWinCount" smallint NOT NULL CHECK ("bWinCount" BETWEEN 0 AND 255),
    "dwRewardMoney" integer NOT NULL,
    "wRewardItem_1" smallint NOT NULL,
    "wRewardItem_2" smallint NOT NULL,
    "bItemCount_1" smallint NOT NULL CHECK ("bItemCount_1" BETWEEN 0 AND 255),
    "bItemCount_2" smallint NOT NULL CHECK ("bItemCount_2" BETWEEN 0 AND 255),
    "bProb_Win" smallint NOT NULL CHECK ("bProb_Win" BETWEEN 0 AND 255),
    "bProb_Draw" smallint NOT NULL CHECK ("bProb_Draw" BETWEEN 0 AND 255),
    "bProb_Lose" smallint NOT NULL CHECK ("bProb_Lose" BETWEEN 0 AND 255),
    "wWinKeep" smallint NOT NULL,
    "wWinPeriod" smallint NOT NULL,
    "wItemID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bType", "bWinCount")
);

CREATE TABLE "legacy_game"."TRPSGAMERECORDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bWinCount" smallint NOT NULL CHECK ("bWinCount" BETWEEN 0 AND 255),
    "dWinDate" timestamp without time zone NOT NULL,
    "dwCharID" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSAVEDCUSTOMCLOAKTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSKILLCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "szName" text NOT NULL,
    "wPrevActiveID" smallint NOT NULL,
    "wParentSkillID" smallint NOT NULL,
    "wItemID" smallint NOT NULL,
    "wMaxRange" smallint NOT NULL,
    "wMinRange" smallint NOT NULL,
    "wPosture" smallint NOT NULL,
    "dwConditionID" integer NOT NULL,
    "dwWeaponID" integer NOT NULL,
    "dwClassID" integer NOT NULL,
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    "fPrice" double precision NOT NULL,
    "dwUseMP" integer NOT NULL,
    "bUseMPType" smallint NOT NULL CHECK ("bUseMPType" BETWEEN 0 AND 255),
    "dwUseHP" integer NOT NULL,
    "bUseHPType" smallint NOT NULL CHECK ("bUseHPType" BETWEEN 0 AND 255),
    "dwReuseDelay" integer NOT NULL,
    "nReuseDelayInc" integer NOT NULL,
    "dwLoopDelay" integer NOT NULL,
    "dwActionTime" integer NOT NULL,
    "dwDuration" integer NOT NULL,
    "dwDurationInc" integer NOT NULL,
    "dwKindDelay" integer NOT NULL,
    "dwAggro" integer NOT NULL,
    "dwAggroInc" integer NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bMaxLevel" smallint NOT NULL CHECK ("bMaxLevel" BETWEEN 0 AND 255),
    "bNextLevel" smallint NOT NULL CHECK ("bNextLevel" BETWEEN 0 AND 255),
    "bTarget" smallint NOT NULL CHECK ("bTarget" BETWEEN 0 AND 255),
    "bTargetRange" smallint NOT NULL CHECK ("bTargetRange" BETWEEN 0 AND 255),
    "bIsuse" smallint NOT NULL CHECK ("bIsuse" BETWEEN 0 AND 255),
    "bTargetHit" smallint NOT NULL CHECK ("bTargetHit" BETWEEN 0 AND 255),
    "bPositive" smallint NOT NULL CHECK ("bPositive" BETWEEN 0 AND 255),
    "bPriority" smallint NOT NULL CHECK ("bPriority" BETWEEN 0 AND 255),
    "bSpeedApply" smallint NOT NULL CHECK ("bSpeedApply" BETWEEN 0 AND 255),
    "bCanLearn" smallint NOT NULL CHECK ("bCanLearn" BETWEEN 0 AND 255),
    "bORadius" smallint NOT NULL CHECK ("bORadius" BETWEEN 0 AND 255),
    "bIsRide" smallint NOT NULL CHECK ("bIsRide" BETWEEN 0 AND 255),
    "bIsDismount" smallint NOT NULL CHECK ("bIsDismount" BETWEEN 0 AND 255),
    "wTargetActiveID" smallint NOT NULL,
    "bMaintainType" smallint NOT NULL CHECK ("bMaintainType" BETWEEN 0 AND 255),
    "bDuraSlot" smallint NOT NULL CHECK ("bDuraSlot" BETWEEN 0 AND 255),
    "bCanCancel" smallint NOT NULL CHECK ("bCanCancel" BETWEEN 0 AND 255),
    "bHitTest" smallint NOT NULL CHECK ("bHitTest" BETWEEN 0 AND 255),
    "bHitInit" smallint NOT NULL CHECK ("bHitInit" BETWEEN 0 AND 255),
    "bHitInc" smallint NOT NULL CHECK ("bHitInc" BETWEEN 0 AND 255),
    "bGlobal" smallint NOT NULL CHECK ("bGlobal" BETWEEN 0 AND 255),
    "bRadius" smallint NOT NULL CHECK ("bRadius" BETWEEN 0 AND 255),
    "bStatic" smallint NOT NULL CHECK ("bStatic" BETWEEN 0 AND 255),
    "bEraseAct" smallint NOT NULL CHECK ("bEraseAct" BETWEEN 0 AND 255),
    "bEraseHide" smallint NOT NULL CHECK ("bEraseHide" BETWEEN 0 AND 255),
    "bIsHideSkill" smallint NOT NULL CHECK ("bIsHideSkill" BETWEEN 0 AND 255),
    "bRunFromServer" smallint NOT NULL CHECK ("bRunFromServer" BETWEEN 0 AND 255),
    "bCheckAttacker" smallint NOT NULL CHECK ("bCheckAttacker" BETWEEN 0 AND 255),
    "wTriggerID" smallint NOT NULL,
    "wMapID" smallint NOT NULL,
    "bRepeatCount" smallint CHECK ("bRepeatCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TSKILLDATA" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wSkillID" smallint NOT NULL,
    "bAction" smallint NOT NULL CHECK ("bAction" BETWEEN 0 AND 255),
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "bAttr" smallint NOT NULL CHECK ("bAttr" BETWEEN 0 AND 255),
    "bExec" smallint NOT NULL CHECK ("bExec" BETWEEN 0 AND 255),
    "bInc" smallint NOT NULL CHECK ("bInc" BETWEEN 0 AND 255),
    "wValue" smallint NOT NULL,
    "wValueInc" smallint NOT NULL,
    "bCalc" smallint NOT NULL CHECK ("bCalc" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wSkillID", "bAction", "bType", "bAttr", "bExec")
);

CREATE TABLE "legacy_game"."TSKILLLOG" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwGold" integer NOT NULL,
    "dwSilver" integer NOT NULL,
    "dwCooper" integer NOT NULL,
    "wSkill" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "timeInsert" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSKILLMAINTAINTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wSkillID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwRemainTick" integer NOT NULL,
    "bAttackType" smallint NOT NULL CHECK ("bAttackType" BETWEEN 0 AND 255),
    "dwAttackID" integer NOT NULL,
    "bHostType" smallint NOT NULL CHECK ("bHostType" BETWEEN 0 AND 255),
    "dwHostID" integer NOT NULL,
    "bAttackCountry" smallint NOT NULL CHECK ("bAttackCountry" BETWEEN 0 AND 255),
    "fPosX" real NOT NULL,
    "fPosY" real NOT NULL,
    "fPosZ" real NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "wSkillID")
);

CREATE TABLE "legacy_game"."TSKILLPOINTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bSkillPoint" smallint NOT NULL CHECK ("bSkillPoint" BETWEEN 0 AND 255),
    "bGroupPoint" smallint NOT NULL CHECK ("bGroupPoint" BETWEEN 0 AND 255),
    "bPrevSkillLevel" smallint NOT NULL CHECK ("bPrevSkillLevel" BETWEEN 0 AND 255),
    "dwPayback" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID", "bLevel")
);

CREATE TABLE "legacy_game"."TSKILLREWARD" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwMoney" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID", "bLevel")
);

CREATE TABLE "legacy_game"."TSKILLREWARDMONEY" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwMoney" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSKILLTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wSkillID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwRemainTick" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSKYGARDENTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "dateWarTime" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TSOULMATETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwTarget" integer NOT NULL,
    "dwTime" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TSPAWNPATHCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wSpawnID" smallint NOT NULL,
    "bPathID" smallint NOT NULL CHECK ("bPathID" BETWEEN 0 AND 255),
    "fPosX" real NOT NULL,
    "fPosY" real NOT NULL,
    "fPosZ" real NOT NULL,
    "bProb" smallint NOT NULL CHECK ("bProb" BETWEEN 0 AND 255),
    "fRadius" real NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wSpawnID", "bPathID")
);

CREATE TABLE "legacy_game"."TSPAWNPOSCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "wMapID" smallint NOT NULL,
    "fPosX" real NOT NULL,
    "fPosY" real NOT NULL,
    "fPosZ" real NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TSPECIALBOXCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "wGroup" smallint,
    "wItemID" smallint,
    "bLevel" smallint CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint CHECK ("bGLevel" BETWEEN 0 AND 255),
    "dwDuraMax" integer,
    "dwDuraCur" integer,
    "bRefineCur" smallint CHECK ("bRefineCur" BETWEEN 0 AND 255),
    "bGradeEffect" smallint CHECK ("bGradeEffect" BETWEEN 0 AND 255),
    "bMagic1" smallint CHECK ("bMagic1" BETWEEN 0 AND 255),
    "bMagic2" smallint CHECK ("bMagic2" BETWEEN 0 AND 255),
    "bMagic3" smallint CHECK ("bMagic3" BETWEEN 0 AND 255),
    "bMagic4" smallint CHECK ("bMagic4" BETWEEN 0 AND 255),
    "bMagic5" smallint CHECK ("bMagic5" BETWEEN 0 AND 255),
    "bMagic6" smallint CHECK ("bMagic6" BETWEEN 0 AND 255),
    "wValue1" smallint,
    "wValue2" smallint,
    "wValue3" smallint,
    "wValue4" smallint,
    "wValue5" smallint,
    "wValue6" smallint,
    "dwTime1" integer,
    "dwTime2" integer,
    "dwTime3" integer,
    "dwTime4" integer,
    "dwTime5" integer,
    "dwTime6" integer,
    "bGem" smallint CHECK ("bGem" BETWEEN 0 AND 255),
    "wMoggItemID" smallint,
    "wUseTime" smallint,
    "bClass" text,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSTARTHOTKEY" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bClassID" smallint NOT NULL CHECK ("bClassID" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSTARTITEMCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "bClass" smallint NOT NULL CHECK ("bClass" BETWEEN 0 AND 255),
    "bInven" smallint NOT NULL CHECK ("bInven" BETWEEN 0 AND 255),
    "bSlot" smallint NOT NULL CHECK ("bSlot" BETWEEN 0 AND 255),
    "bChartType" smallint NOT NULL CHECK ("bChartType" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bCountry", "bClass", "bInven", "bSlot")
);

CREATE TABLE "legacy_game"."TSTARTRECALL" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bClassID" smallint NOT NULL CHECK ("bClassID" BETWEEN 0 AND 255),
    "bCountryID" smallint NOT NULL CHECK ("bCountryID" BETWEEN 0 AND 255),
    "wMonID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSTARTSKILL" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bClassID" smallint NOT NULL CHECK ("bClassID" BETWEEN 0 AND 255),
    "wSkillID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TSVRMSGCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwID" integer NOT NULL,
    "szMessage" text NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwID")
);

CREATE TABLE "legacy_game"."TSWITCHCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwSwitchID" integer NOT NULL,
    "wMapID" smallint NOT NULL,
    "wPosX" smallint NOT NULL,
    "wPosY" smallint NOT NULL,
    "wPosZ" smallint NOT NULL,
    "bStart" smallint NOT NULL CHECK ("bStart" BETWEEN 0 AND 255),
    "bLockOnOpen" smallint NOT NULL CHECK ("bLockOnOpen" BETWEEN 0 AND 255),
    "bLockOnClose" smallint NOT NULL CHECK ("bLockOnClose" BETWEEN 0 AND 255),
    "dwDuration" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwSwitchID")
);

CREATE TABLE "legacy_game"."TTAXTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bCountry" smallint NOT NULL CHECK ("bCountry" BETWEEN 0 AND 255),
    "wNpcID" smallint NOT NULL,
    "dwMoney" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TTEMPCABINETITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bCabinetID" smallint NOT NULL CHECK ("bCabinetID" BETWEEN 0 AND 255),
    "dwStItemID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    "bGLevel" smallint NOT NULL CHECK ("bGLevel" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bCabinetID", "dwStItemID")
);

CREATE TABLE "legacy_game"."TTEMPCABINETTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bCabinetID" smallint NOT NULL CHECK ("bCabinetID" BETWEEN 0 AND 255),
    "bUse" smallint NOT NULL CHECK ("bUse" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bCabinetID")
);

CREATE TABLE "legacy_game"."TTEMPEXPITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwRemainTime" integer NOT NULL,
    "dEndTime" timestamp without time zone NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TTEMPINVENTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "bInvenID" smallint NOT NULL CHECK ("bInvenID" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "dEndTime" timestamp without time zone NOT NULL,
    "bELD" smallint NOT NULL CHECK ("bELD" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "bInvenID")
);

CREATE TABLE "legacy_game"."TTEMPITEMTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dlID" bigint NOT NULL,
    "bStorageType" smallint NOT NULL CHECK ("bStorageType" BETWEEN 0 AND 255),
    "dwStorageID" integer NOT NULL,
    "bOwnerType" smallint NOT NULL CHECK ("bOwnerType" BETWEEN 0 AND 255),
    "dwOwnerID" integer NOT NULL,
    "bItemID" smallint NOT NULL CHECK ("bItemID" BETWEEN 0 AND 255),
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
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TTEMPITEMUSEDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wDelayGroupID" smallint NOT NULL,
    "dwTick" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "wDelayGroupID")
);

CREATE TABLE "legacy_game"."TTEMPSKILLMAINTAINTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wSkillID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwRemainTick" integer NOT NULL,
    "bAttackType" smallint NOT NULL CHECK ("bAttackType" BETWEEN 0 AND 255),
    "dwAttackID" integer NOT NULL,
    "bHostType" smallint NOT NULL CHECK ("bHostType" BETWEEN 0 AND 255),
    "dwHostID" integer NOT NULL,
    "bAttackCountry" smallint NOT NULL CHECK ("bAttackCountry" BETWEEN 0 AND 255),
    "fPosX" real NOT NULL,
    "fPosY" real NOT NULL,
    "fPosZ" real NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "wSkillID")
);

CREATE TABLE "legacy_game"."TTEMPSKILLTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wSkillID" smallint NOT NULL,
    "bLevel" smallint NOT NULL CHECK ("bLevel" BETWEEN 0 AND 255),
    "dwRemainTick" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "wSkillID")
);

CREATE TABLE "legacy_game"."TTITLECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wTitleID" smallint NOT NULL,
    "wCategory" smallint NOT NULL,
    "strTitle" text NOT NULL,
    "bKind" smallint NOT NULL CHECK ("bKind" BETWEEN 0 AND 255),
    "dwRequirement" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wTitleID")
);

CREATE TABLE "legacy_game"."TTITLETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "wTitleID" smallint NOT NULL,
    "bSelected" smallint NOT NULL CHECK ("bSelected" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID", "wTitleID")
);

CREATE TABLE "legacy_game"."TTNMTEVENTREWARDTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "wTournamentID" smallint NOT NULL,
    "bEntryID" smallint NOT NULL CHECK ("bEntryID" BETWEEN 0 AND 255),
    "dwClass" integer NOT NULL,
    "bCheckShield" smallint NOT NULL CHECK ("bCheckShield" BETWEEN 0 AND 255),
    "bChartType" smallint NOT NULL CHECK ("bChartType" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bItemCount" smallint NOT NULL CHECK ("bItemCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TTNMTEVENTSCHEDULETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wTournamentID" smallint NOT NULL,
    "bStep" smallint NOT NULL CHECK ("bStep" BETWEEN 0 AND 255),
    "dwPeriod" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wTournamentID", "bStep")
);

CREATE TABLE "legacy_game"."TTNMTEVENTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wTournamentID" smallint NOT NULL,
    "bEntryID" smallint NOT NULL CHECK ("bEntryID" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwClass" integer NOT NULL,
    "dwFee" integer NOT NULL,
    "dwFeeBack" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bItemCount" smallint NOT NULL CHECK ("bItemCount" BETWEEN 0 AND 255),
    "bMaxLevel" smallint NOT NULL CHECK ("bMaxLevel" BETWEEN 0 AND 255),
    "bMinLevel" smallint NOT NULL CHECK ("bMinLevel" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wTournamentID", "bEntryID")
);

CREATE TABLE "legacy_game"."TTNMTEVENTTIMETABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wTournamentID" smallint NOT NULL,
    "bWeek" smallint NOT NULL CHECK ("bWeek" BETWEEN 0 AND 255),
    "bDay" smallint NOT NULL CHECK ("bDay" BETWEEN 0 AND 255),
    "dwStart" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wTournamentID")
);

CREATE TABLE "legacy_game"."TTOURNAMENTCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bEntryID" smallint NOT NULL CHECK ("bEntryID" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "bType" smallint NOT NULL CHECK ("bType" BETWEEN 0 AND 255),
    "dwClass" integer NOT NULL,
    "dwFee" integer NOT NULL,
    "dwFeeBack" integer NOT NULL,
    "wItemID" smallint NOT NULL,
    "bItemCount" smallint NOT NULL CHECK ("bItemCount" BETWEEN 0 AND 255),
    "bEnable" smallint NOT NULL CHECK ("bEnable" BETWEEN 0 AND 255),
    "bGroup" smallint NOT NULL CHECK ("bGroup" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bEntryID")
);

CREATE TABLE "legacy_game"."TTOURNAMENTPLAYERTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwChiefID" integer NOT NULL,
    "bEntry" smallint NOT NULL CHECK ("bEntry" BETWEEN 0 AND 255),
    "bStep" smallint NOT NULL CHECK ("bStep" BETWEEN 0 AND 255),
    "bResult" smallint NOT NULL CHECK ("bResult" BETWEEN 0 AND 255),
    "szHWID" text,
    "dwIPAddr" integer,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game"."TTOURNAMENTREWARDCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bEntryID" smallint NOT NULL CHECK ("bEntryID" BETWEEN 0 AND 255),
    "dwClass" integer NOT NULL,
    "bCheckShield" smallint NOT NULL CHECK ("bCheckShield" BETWEEN 0 AND 255),
    "bChartType" smallint NOT NULL CHECK ("bChartType" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bItemCount" smallint NOT NULL CHECK ("bItemCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TTOURNAMENTREWARDCHART_weap" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bEntryID" smallint NOT NULL CHECK ("bEntryID" BETWEEN 0 AND 255),
    "dwClass" integer NOT NULL,
    "bCheckShield" smallint NOT NULL CHECK ("bCheckShield" BETWEEN 0 AND 255),
    "bChartType" smallint NOT NULL CHECK ("bChartType" BETWEEN 0 AND 255),
    "wItemID" smallint NOT NULL,
    "bItemCount" smallint NOT NULL CHECK ("bItemCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "wID")
);

CREATE TABLE "legacy_game"."TTOURNAMENTSCHEDULECHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bStep" smallint NOT NULL CHECK ("bStep" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    "dwPeriod" integer NOT NULL,
    "bGroup" smallint NOT NULL CHECK ("bGroup" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bStep", "bGroup")
);

CREATE TABLE "legacy_game"."TTOURNAMENTSTATUSTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "wID" smallint NOT NULL,
    "bGroup" smallint NOT NULL CHECK ("bGroup" BETWEEN 0 AND 255),
    "bStep" smallint NOT NULL CHECK ("bStep" BETWEEN 0 AND 255),
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."TUNIFYPET" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwUserID" integer NOT NULL,
    "wPetID" smallint NOT NULL,
    "dwSecond" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwUserID", "wPetID")
);

CREATE TABLE "legacy_game"."TUNITCHART" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "bGroup" smallint NOT NULL CHECK ("bGroup" BETWEEN 0 AND 255),
    "bServerID" smallint NOT NULL CHECK ("bServerID" BETWEEN 0 AND 255),
    "wMapID" smallint NOT NULL,
    "wUnitID" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "bGroup", "bServerID", "wMapID", "wUnitID")
);

CREATE TABLE "legacy_game"."charkilling_log" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwKillerID" integer,
    "dwTargetID" integer,
    "date" timestamp without time zone,
    PRIMARY KEY ("_import_run_id", "_source_row_number")
);

CREATE TABLE "legacy_game"."dtproperties" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "id" integer NOT NULL,
    "objectid" integer,
    "property" text NOT NULL,
    "value" text,
    "uvalue" text,
    "lvalue" bytea,
    "version" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "id", "property")
);

CREATE TABLE "legacy_game_tgame"."TLASTMONTHPOINTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwRank" integer NOT NULL,
    "dwPoint" integer NOT NULL,
    "wWin" smallint NOT NULL,
    "wLose" smallint NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);

CREATE TABLE "legacy_game_tgame"."TLASTTOTALPOINTTABLE" (
    "_import_run_id" bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    "_source_row_number" bigint NOT NULL CHECK ("_source_row_number" > 0),
    "dwCharID" integer NOT NULL,
    "dwRank" integer NOT NULL,
    "dwPoint" integer NOT NULL,
    "dwWin" integer NOT NULL,
    "dwLose" integer NOT NULL,
    PRIMARY KEY ("_import_run_id", "_source_row_number"),
    UNIQUE ("_import_run_id", "dwCharID")
);
