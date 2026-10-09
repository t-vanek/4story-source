-- Character reference publication, independent of the existing Map release.
-- Source values/types stay in immutable legacy snapshots. Runtime views are read-only.
CREATE SCHEMA character_compat;
CREATE TABLE runtime_control.character_catalog (
    singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE runtime_control.character_catalog_activations (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    previous_run_id bigint REFERENCES reconstruction.import_runs(id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL,
    activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE VIEW character_compat.catalog_release AS
SELECT a.run_id, r.manifest_sha256, r.status FROM runtime_control.character_catalog a
JOIN reconstruction.import_runs r ON r.id=a.run_id WHERE a.singleton;
CREATE VIEW character_compat."TVETERANCHART" AS
SELECT s."_import_run_id" AS release_id, s."bID", s."bLevel"
FROM "legacy_global"."TVETERANCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TCLASSCHART" AS
SELECT s."_import_run_id" AS release_id, s."bClassID", s."wSTR", s."wDEX", s."wCON", s."wINT", s."wWIS", s."wMEN"
FROM "legacy_game"."TCLASSCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TITEMCHART" AS
SELECT s."_import_run_id" AS release_id, s."wItemID", s."bType", s."bKind", s."szNAME", s."wAttrID", s."wUseValue", s."dwSlotID", s."dwClassID", s."bPrmSlotID", s."bSubSlotID", s."bLevel", s."fPrice", s."bIsSell", s."bMinRange", s."bMaxRange", s."bStack", s."bEquipSkill", s."bSlotCount", s."bUseItemKind", s."bUseItemCount", s."bGrade", s."wUseTime", s."bUseType", s."bCanGrade", s."bCanMagic", s."bCanRare", s."bDropLevel", s."dwSpeedInc", s."bItemCountry", s."bIsSpecial", s."dwDelay", s."fRevision", s."fMRevision", s."fAtRate", s."fMAtRate", s."bCanGamble", s."wItemProb_G", s."bDestroyProb", s."bGambleProb", s."dwDuraMax", s."bRefineMax", s."bCanRepair", s."wDelayGroupID", s."wWeight", s."bGroupID", s."bInitState", s."bCanWrap", s."dwCode", s."bCanColor", s."fPvPrice", s."bConsumable", s."wExpandValue"
FROM "legacy_game"."TITEMCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TLEVELCHART" AS
SELECT s."_import_run_id" AS release_id, s."bLevel", s."dwEXP", s."dwHP", s."dwMP", s."bSkillPoint", s."dwMoney", s."dwScore", s."dwRegCost", s."dwSearchCost", s."dwGambleCost", s."dwRepCost", s."dwRepairCost", s."dwRefineCost", s."wPvPoint", s."dwPvPMoney", s."dwPvPExp"
FROM "legacy_game"."TLEVELCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TMONATTRCHART" AS
SELECT s."_import_run_id" AS release_id, s."wID", s."bLevel", s."wAP", s."wLAP", s."dwAtkSpeed", s."wAL", s."wDL", s."bCriticalPP", s."dwMaxHP", s."bHPRecover", s."wMAP", s."bCriticalMP", s."dwMaxMP", s."bMPRecover", s."wDP", s."wMDP", s."wMinWAP", s."wMaxWAP", s."wWDP", s."wMAL", s."wMDL"
FROM "legacy_game"."TMONATTRCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TMONSTERCHART" AS
SELECT s."_import_run_id" AS release_id, s."wID", s."szName", s."szName2", s."bRace", s."bClass", s."wKind", s."bLevel", s."bAIType", s."bRange", s."wChaseRange", s."bRoamProb", s."bMoneyProb", s."dwMinMoney", s."dwMaxMoney", s."bItemProb", s."bDropCount", s."wExp", s."bIsSelf", s."bRecallType", s."bCanSelect", s."bCanAttack", s."bTame", s."bCall", s."bIsSpecial", s."bRemove", s."wMonAttr", s."wSummonAttr", s."wTransSkillID", s."fSize", s."wSkill1", s."wSkill2", s."wSkill3", s."wSkill4"
FROM "legacy_game"."TMONSTERCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TNPCCHART" AS
SELECT s."_import_run_id" AS release_id, s."wID", s."NC_szName2", s."szName", s."bType", s."dwClass", s."bCountryID", s."wLocalID", s."bCondition", s."bDiscountRate", s."bAddProb", s."wItemID", s."wMapID", s."fPosX", s."fPosY", s."fPosZ", s."szLocal"
FROM "legacy_game"."TNPCCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TQUESTITEMCHART" AS
SELECT s."_import_run_id" AS release_id, s."dwID", s."wItemID", s."bLevel", s."bGLevel", s."bDropLevel", s."dwDuraMax", s."dwDuraCur", s."bRefineCur", s."wUseTime", s."bGradeEffect", s."bMagic1", s."bMagic2", s."bMagic3", s."bMagic4", s."bMagic5", s."bMagic6", s."wValue1", s."wValue2", s."wValue3", s."wValue4", s."wValue5", s."wValue6", s."dwTime1", s."dwTime2", s."dwTime3", s."dwTime4", s."dwTime5", s."dwTime6", s."dwMoney", s."bGem"
FROM "legacy_game"."TQUESTITEMCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TRACECHART" AS
SELECT s."_import_run_id" AS release_id, s."bRaceID", s."wSTR", s."wDEX", s."wCON", s."wINT", s."wWIS", s."wMEN"
FROM "legacy_game"."TRACECHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TSTARTHOTKEY" AS
SELECT s."_import_run_id" AS release_id, s."bClassID", s."bInvenID", s."bType1", s."wID1", s."bType2", s."wID2", s."bType3", s."wID3", s."bType4", s."wID4", s."bType5", s."wID5", s."bType6", s."wID6", s."bType7", s."wID7", s."bType8", s."wID8", s."bType9", s."wID9", s."bType10", s."wID10", s."bType11", s."wID11", s."bType12", s."wID12"
FROM "legacy_game"."TSTARTHOTKEY" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TSTARTITEMCHART" AS
SELECT s."_import_run_id" AS release_id, s."bCountry", s."bClass", s."bInven", s."bSlot", s."bChartType", s."wItemID", s."bCount"
FROM "legacy_game"."TSTARTITEMCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TSTARTRECALL" AS
SELECT s."_import_run_id" AS release_id, s."bClassID", s."bCountryID", s."wMonID"
FROM "legacy_game"."TSTARTRECALL" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TSTARTSKILL" AS
SELECT s."_import_run_id" AS release_id, s."bClassID", s."wSkillID", s."bLevel"
FROM "legacy_game"."TSTARTSKILL" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
REVOKE ALL ON SCHEMA character_compat FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA character_compat FROM PUBLIC;
REVOKE ALL ON runtime_control.character_catalog, runtime_control.character_catalog_activations FROM PUBLIC;
