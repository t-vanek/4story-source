-- Migration 006: derived read-only current C++ catalog contracts.

-- Metadata SHA256 62fc3fe7cb2ae38a194cd402b508eea84d9892b85e877f3082a5d90d75278f21.

-- Do not edit applied SQL; add a new migration for a changed contract.

-- No original snapshot row or type is modified.

CREATE VIEW game_compat."TCLASSCHART" AS
SELECT a."bClassID", a."wSTR", a."wDEX", a."wCON", a."wINT", a."wWIS", a."wMEN"
FROM "legacy_game"."TCLASSCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TCLASSCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tclasschart" AS
SELECT "bClassID" AS "bclassid", "wSTR" AS "wstr", "wDEX" AS "wdex", "wCON" AS "wcon", "wINT" AS "wint", "wWIS" AS "wwis", "wMEN" AS "wmen"
FROM game_compat."TCLASSCHART";

CREATE VIEW game_compat."TFORMULACHART" AS
SELECT a."bID", a."szName", a."dwinit", a."fRateX", a."fRateY"
FROM "legacy_game"."TFORMULACHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TFORMULACHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tformulachart" AS
SELECT "bID" AS "bid", "szName" AS "szname", "dwinit" AS "dwinit", "fRateX" AS "fratex", "fRateY" AS "fratey"
FROM game_compat."TFORMULACHART";

CREATE VIEW game_compat."TITEMCHART" AS
SELECT a."wItemID", a."bType", a."bKind", a."szNAME", a."wAttrID", a."wUseValue", a."dwSlotID", a."dwClassID", a."bPrmSlotID", a."bSubSlotID", a."bLevel", a."fPrice", a."bIsSell", a."bMinRange", a."bMaxRange", a."bStack", a."bEquipSkill", a."bSlotCount", a."bUseItemKind", a."bUseItemCount", a."bGrade", a."wUseTime", a."bUseType", a."bCanGrade", a."bCanMagic", a."bCanRare", a."bDropLevel", a."dwSpeedInc", a."bItemCountry", a."bIsSpecial", a."dwDelay", a."fRevision", a."fMRevision", a."fAtRate", a."fMAtRate", a."bCanGamble", a."wItemProb_G", a."bDestroyProb", a."bGambleProb", a."dwDuraMax", a."bRefineMax", a."bCanRepair", a."wDelayGroupID", a."wWeight", a."bGroupID", a."bInitState", a."bCanWrap", a."dwCode", a."bCanColor", a."fPvPrice", a."bConsumable", a."wExpandValue"
FROM "legacy_game"."TITEMCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TITEMCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."titemchart" AS
SELECT "wItemID" AS "witemid", "bType" AS "btype", "bKind" AS "bkind", "szNAME" AS "szname", "wAttrID" AS "wattrid", "wUseValue" AS "wusevalue", "dwSlotID" AS "dwslotid", "dwClassID" AS "dwclassid", "bPrmSlotID" AS "bprmslotid", "bSubSlotID" AS "bsubslotid", "bLevel" AS "blevel", "fPrice" AS "fprice", "bIsSell" AS "bissell", "bMinRange" AS "bminrange", "bMaxRange" AS "bmaxrange", "bStack" AS "bstack", "bEquipSkill" AS "bequipskill", "bSlotCount" AS "bslotcount", "bUseItemKind" AS "buseitemkind", "bUseItemCount" AS "buseitemcount", "bGrade" AS "bgrade", "wUseTime" AS "wusetime", "bUseType" AS "busetype", "bCanGrade" AS "bcangrade", "bCanMagic" AS "bcanmagic", "bCanRare" AS "bcanrare", "bDropLevel" AS "bdroplevel", "dwSpeedInc" AS "dwspeedinc", "bItemCountry" AS "bitemcountry", "bIsSpecial" AS "bisspecial", "dwDelay" AS "dwdelay", "fRevision" AS "frevision", "fMRevision" AS "fmrevision", "fAtRate" AS "fatrate", "fMAtRate" AS "fmatrate", "bCanGamble" AS "bcangamble", "wItemProb_G" AS "witemprob_g", "bDestroyProb" AS "bdestroyprob", "bGambleProb" AS "bgambleprob", "dwDuraMax" AS "dwduramax", "bRefineMax" AS "brefinemax", "bCanRepair" AS "bcanrepair", "wDelayGroupID" AS "wdelaygroupid", "wWeight" AS "wweight", "bGroupID" AS "bgroupid", "bInitState" AS "binitstate", "bCanWrap" AS "bcanwrap", "dwCode" AS "dwcode", "bCanColor" AS "bcancolor", "fPvPrice" AS "fpvprice", "bConsumable" AS "bconsumable", "wExpandValue" AS "wexpandvalue"
FROM game_compat."TITEMCHART";

CREATE VIEW game_compat."TMAPMONCHART" AS
SELECT a."wSpawnID", a."wMonID", a."bEssential", a."bLeader", a."bProb"
FROM "legacy_game"."TMAPMONCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TMAPMONCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tmapmonchart" AS
SELECT "wSpawnID" AS "wspawnid", "wMonID" AS "wmonid", "bEssential" AS "bessential", "bLeader" AS "bleader", "bProb" AS "bprob"
FROM game_compat."TMAPMONCHART";

CREATE VIEW game_compat."TMONATTRCHART" AS
SELECT m."wID" AS "wID", a."bLevel", a."wAP", a."wLAP", a."dwAtkSpeed", a."wAL", a."wDL", a."bCriticalPP", a."dwMaxHP", a."bHPRecover", a."wMAP", a."bCriticalMP", a."dwMaxMP", a."bMPRecover", a."wDP", a."wMDP", a."wMinWAP", a."wMaxWAP", a."wWDP", a."wMAL", a."wMDL"
FROM legacy_game."TMONATTRCHART" a
JOIN legacy_game."TMONSTERCHART" m
  ON m."_import_run_id" = a."_import_run_id"
 AND m."wMonAttr" = a."wID" AND m."bLevel" = a."bLevel"
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TMONATTRCHART" IS 'PROPOSED compatibility transform: wID is monster ID; stats come from the backup attribute family wMonAttr at the template bLevel. Legacy TMap.cpp FindMonAttr and current spawn_manager.cpp attrs.Find. No missing stats are fabricated.';

CREATE VIEW game_compat."tmonattrchart" AS
SELECT "wID" AS "wid", "bLevel" AS "blevel", "wAP" AS "wap", "wLAP" AS "wlap", "dwAtkSpeed" AS "dwatkspeed", "wAL" AS "wal", "wDL" AS "wdl", "bCriticalPP" AS "bcriticalpp", "dwMaxHP" AS "dwmaxhp", "bHPRecover" AS "bhprecover", "wMAP" AS "wmap", "bCriticalMP" AS "bcriticalmp", "dwMaxMP" AS "dwmaxmp", "bMPRecover" AS "bmprecover", "wDP" AS "wdp", "wMDP" AS "wmdp", "wMinWAP" AS "wminwap", "wMaxWAP" AS "wmaxwap", "wWDP" AS "wwdp", "wMAL" AS "wmal", "wMDL" AS "wmdl"
FROM game_compat."TMONATTRCHART";

CREATE VIEW game_compat."TMONITEMCHART" AS
SELECT a."bChartType", a."wMonID", a."wItemID", a."wItemIDMin", a."wItemIDMax", a."bLevelMin", a."bLevelMax", a."bItemProb_N1", a."bItemProb_N2", a."bItemProb_N3", a."bItemProb_N4", a."bItemProb_M", a."bItemProb_S", a."bItemProb_R", a."bItemMagicOpt", a."bItemRareOpt", a."wWeight"
FROM "legacy_game"."TMONITEMCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TMONITEMCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tmonitemchart" AS
SELECT "bChartType" AS "bcharttype", "wMonID" AS "wmonid", "wItemID" AS "witemid", "wItemIDMin" AS "witemidmin", "wItemIDMax" AS "witemidmax", "bLevelMin" AS "blevelmin", "bLevelMax" AS "blevelmax", "bItemProb_N1" AS "bitemprob_n1", "bItemProb_N2" AS "bitemprob_n2", "bItemProb_N3" AS "bitemprob_n3", "bItemProb_N4" AS "bitemprob_n4", "bItemProb_M" AS "bitemprob_m", "bItemProb_S" AS "bitemprob_s", "bItemProb_R" AS "bitemprob_r", "bItemMagicOpt" AS "bitemmagicopt", "bItemRareOpt" AS "bitemrareopt", "wWeight" AS "wweight"
FROM game_compat."TMONITEMCHART";

CREATE VIEW game_compat."TMONSPAWNCHART" AS
SELECT a."wID", a."wGroup", a."wLocalID", a."wMapID", a."fPosX", a."fPosY", a."fPosZ", a."wDir", a."bCountry", a."bCount", a."bRange", a."bArea", a."bLink", a."bProb", a."bRoamType", a."dwRegion", a."dwDelay", a."bEvent", a."wPartyID"
FROM "legacy_game"."TMONSPAWNCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TMONSPAWNCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tmonspawnchart" AS
SELECT "wID" AS "wid", "wGroup" AS "wgroup", "wLocalID" AS "wlocalid", "wMapID" AS "wmapid", "fPosX" AS "fposx", "fPosY" AS "fposy", "fPosZ" AS "fposz", "wDir" AS "wdir", "bCountry" AS "bcountry", "bCount" AS "bcount", "bRange" AS "brange", "bArea" AS "barea", "bLink" AS "blink", "bProb" AS "bprob", "bRoamType" AS "broamtype", "dwRegion" AS "dwregion", "dwDelay" AS "dwdelay", "bEvent" AS "bevent", "wPartyID" AS "wpartyid"
FROM game_compat."TMONSPAWNCHART";

CREATE VIEW game_compat."TMONSTERCHART" AS
SELECT a."wID", a."szName", a."szName2", a."bRace", a."bClass", a."wKind", a."bLevel", a."bAIType", a."bRange", a."wChaseRange", a."bRoamProb", a."bMoneyProb", a."dwMinMoney", a."dwMaxMoney", a."bItemProb", a."bDropCount", a."wExp", a."bIsSelf", a."bRecallType", a."bCanSelect", a."bCanAttack", a."bTame", a."bCall", a."bIsSpecial", a."bRemove", a."wMonAttr", a."wSummonAttr", a."wTransSkillID", a."fSize", a."wSkill1", a."wSkill2", a."wSkill3", a."wSkill4"
FROM "legacy_game"."TMONSTERCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TMONSTERCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tmonsterchart" AS
SELECT "wID" AS "wid", "szName" AS "szname", "szName2" AS "szname2", "bRace" AS "brace", "bClass" AS "bclass", "wKind" AS "wkind", "bLevel" AS "blevel", "bAIType" AS "baitype", "bRange" AS "brange", "wChaseRange" AS "wchaserange", "bRoamProb" AS "broamprob", "bMoneyProb" AS "bmoneyprob", "dwMinMoney" AS "dwminmoney", "dwMaxMoney" AS "dwmaxmoney", "bItemProb" AS "bitemprob", "bDropCount" AS "bdropcount", "wExp" AS "wexp", "bIsSelf" AS "bisself", "bRecallType" AS "brecalltype", "bCanSelect" AS "bcanselect", "bCanAttack" AS "bcanattack", "bTame" AS "btame", "bCall" AS "bcall", "bIsSpecial" AS "bisspecial", "bRemove" AS "bremove", "wMonAttr" AS "wmonattr", "wSummonAttr" AS "wsummonattr", "wTransSkillID" AS "wtransskillid", "fSize" AS "fsize", "wSkill1" AS "wskill1", "wSkill2" AS "wskill2", "wSkill3" AS "wskill3", "wSkill4" AS "wskill4"
FROM game_compat."TMONSTERCHART";

CREATE VIEW game_compat."TNPCCHART" AS
SELECT a."wID", a."NC_szName2", a."szName", a."bType", a."dwClass", a."bCountryID", a."wLocalID", a."bCondition", a."bDiscountRate", a."bAddProb", a."wItemID", a."wMapID", a."fPosX", a."fPosY", a."fPosZ", a."szLocal"
FROM "legacy_game"."TNPCCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TNPCCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tnpcchart" AS
SELECT "wID" AS "wid", "NC_szName2" AS "nc_szname2", "szName" AS "szname", "bType" AS "btype", "dwClass" AS "dwclass", "bCountryID" AS "bcountryid", "wLocalID" AS "wlocalid", "bCondition" AS "bcondition", "bDiscountRate" AS "bdiscountrate", "bAddProb" AS "baddprob", "wItemID" AS "witemid", "wMapID" AS "wmapid", "fPosX" AS "fposx", "fPosY" AS "fposy", "fPosZ" AS "fposz", "szLocal" AS "szlocal"
FROM game_compat."TNPCCHART";

CREATE VIEW game_compat."TQREWARDCHART" AS
SELECT a."dwID", a."dwQuestID", a."bRewardType", a."dwRewardID", a."bTakeMethod", a."bTakeData", a."bCount", a."dwQuestMob", a."dwQuestTime", a."dwQuestPathMob", a."dwTicketID", a."bSendQ"
FROM "legacy_game"."TQREWARDCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TQREWARDCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tqrewardchart" AS
SELECT "dwID" AS "dwid", "dwQuestID" AS "dwquestid", "bRewardType" AS "brewardtype", "dwRewardID" AS "dwrewardid", "bTakeMethod" AS "btakemethod", "bTakeData" AS "btakedata", "bCount" AS "bcount", "dwQuestMob" AS "dwquestmob", "dwQuestTime" AS "dwquesttime", "dwQuestPathMob" AS "dwquestpathmob", "dwTicketID" AS "dwticketid", "bSendQ" AS "bsendq"
FROM game_compat."TQREWARDCHART";

CREATE VIEW game_compat."TQUESTCHART" AS
SELECT a."dwQuestID", a."dwParentID", a."bType", a."bForceRun", a."bTriggerType", a."dwTriggerID", a."bCountMax", a."bLevel", a."bMain", a."bConditionCheck"
FROM "legacy_game"."TQUESTCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TQUESTCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tquestchart" AS
SELECT "dwQuestID" AS "dwquestid", "dwParentID" AS "dwparentid", "bType" AS "btype", "bForceRun" AS "bforcerun", "bTriggerType" AS "btriggertype", "dwTriggerID" AS "dwtriggerid", "bCountMax" AS "bcountmax", "bLevel" AS "blevel", "bMain" AS "bmain", "bConditionCheck" AS "bconditioncheck"
FROM game_compat."TQUESTCHART";

CREATE VIEW game_compat."TQUESTTERMCHART" AS
SELECT a."dwID", a."dwQuestID", a."bTermType", a."dwTermID", a."bCount"
FROM "legacy_game"."TQUESTTERMCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TQUESTTERMCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tquesttermchart" AS
SELECT "dwID" AS "dwid", "dwQuestID" AS "dwquestid", "bTermType" AS "btermtype", "dwTermID" AS "dwtermid", "bCount" AS "bcount"
FROM game_compat."TQUESTTERMCHART";

CREATE VIEW game_compat."TRACECHART" AS
SELECT a."bRaceID", a."wSTR", a."wDEX", a."wCON", a."wINT", a."wWIS", a."wMEN"
FROM "legacy_game"."TRACECHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TRACECHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tracechart" AS
SELECT "bRaceID" AS "braceid", "wSTR" AS "wstr", "wDEX" AS "wdex", "wCON" AS "wcon", "wINT" AS "wint", "wWIS" AS "wwis", "wMEN" AS "wmen"
FROM game_compat."TRACECHART";

CREATE VIEW game_compat."TSKILLCHART" AS
SELECT a."wID", a."szName", a."wPrevActiveID", a."wParentSkillID", a."wItemID", a."wMaxRange", a."wMinRange", a."wPosture", a."dwConditionID", a."dwWeaponID", a."dwClassID", a."bKind", a."fPrice", a."dwUseMP", a."bUseMPType", a."dwUseHP", a."bUseHPType", a."dwReuseDelay", a."nReuseDelayInc", a."dwLoopDelay", a."dwActionTime", a."dwDuration", a."dwDurationInc", a."dwKindDelay", a."dwAggro", a."dwAggroInc", a."bLevel", a."bMaxLevel", a."bNextLevel", a."bTarget", a."bTargetRange", a."bIsuse", a."bTargetHit", a."bPositive", a."bPriority", a."bSpeedApply", a."bCanLearn", a."bORadius", a."bIsRide", a."bIsDismount", a."wTargetActiveID", a."bMaintainType", a."bDuraSlot", a."bCanCancel", a."bHitTest", a."bHitInit", a."bHitInc", a."bGlobal", a."bRadius", a."bStatic", a."bEraseAct", a."bEraseHide", a."bIsHideSkill", a."bRunFromServer", a."bCheckAttacker", a."wTriggerID", a."wMapID", a."bRepeatCount"
FROM "legacy_game"."TSKILLCHART" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TSKILLCHART" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tskillchart" AS
SELECT "wID" AS "wid", "szName" AS "szname", "wPrevActiveID" AS "wprevactiveid", "wParentSkillID" AS "wparentskillid", "wItemID" AS "witemid", "wMaxRange" AS "wmaxrange", "wMinRange" AS "wminrange", "wPosture" AS "wposture", "dwConditionID" AS "dwconditionid", "dwWeaponID" AS "dwweaponid", "dwClassID" AS "dwclassid", "bKind" AS "bkind", "fPrice" AS "fprice", "dwUseMP" AS "dwusemp", "bUseMPType" AS "busemptype", "dwUseHP" AS "dwusehp", "bUseHPType" AS "busehptype", "dwReuseDelay" AS "dwreusedelay", "nReuseDelayInc" AS "nreusedelayinc", "dwLoopDelay" AS "dwloopdelay", "dwActionTime" AS "dwactiontime", "dwDuration" AS "dwduration", "dwDurationInc" AS "dwdurationinc", "dwKindDelay" AS "dwkinddelay", "dwAggro" AS "dwaggro", "dwAggroInc" AS "dwaggroinc", "bLevel" AS "blevel", "bMaxLevel" AS "bmaxlevel", "bNextLevel" AS "bnextlevel", "bTarget" AS "btarget", "bTargetRange" AS "btargetrange", "bIsuse" AS "bisuse", "bTargetHit" AS "btargethit", "bPositive" AS "bpositive", "bPriority" AS "bpriority", "bSpeedApply" AS "bspeedapply", "bCanLearn" AS "bcanlearn", "bORadius" AS "boradius", "bIsRide" AS "bisride", "bIsDismount" AS "bisdismount", "wTargetActiveID" AS "wtargetactiveid", "bMaintainType" AS "bmaintaintype", "bDuraSlot" AS "bduraslot", "bCanCancel" AS "bcancancel", "bHitTest" AS "bhittest", "bHitInit" AS "bhitinit", "bHitInc" AS "bhitinc", "bGlobal" AS "bglobal", "bRadius" AS "bradius", "bStatic" AS "bstatic", "bEraseAct" AS "beraseact", "bEraseHide" AS "berasehide", "bIsHideSkill" AS "bishideskill", "bRunFromServer" AS "brunfromserver", "bCheckAttacker" AS "bcheckattacker", "wTriggerID" AS "wtriggerid", "wMapID" AS "wmapid", "bRepeatCount" AS "brepeatcount"
FROM game_compat."TSKILLCHART";

CREATE VIEW game_compat."TSKILLDATA" AS
SELECT a."wSkillID", a."bAction", a."bType", a."bAttr", a."bExec", a."bInc", a."wValue", a."wValueInc", a."bCalc"
FROM "legacy_game"."TSKILLDATA" a
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = a."_import_run_id";

COMMENT ON VIEW game_compat."TSKILLDATA" IS 'CONFIRMED backup values, projected from the explicitly activated verified TGAME reference import.';

CREATE VIEW game_compat."tskilldata" AS
SELECT "wSkillID" AS "wskillid", "bAction" AS "baction", "bType" AS "btype", "bAttr" AS "battr", "bExec" AS "bexec", "bInc" AS "binc", "wValue" AS "wvalue", "wValueInc" AS "wvalueinc", "bCalc" AS "bcalc"
FROM game_compat."TSKILLDATA";

CREATE VIEW runtime_control.missing_monster_attributes AS
SELECT m."wID" AS monster_id, m."wMonAttr" AS attribute_id, m."bLevel" AS level
FROM legacy_game."TMONSTERCHART" m
JOIN runtime_control.active_catalog r ON r.singleton AND r.run_id = m."_import_run_id"
LEFT JOIN legacy_game."TMONATTRCHART" a
  ON a."_import_run_id" = m."_import_run_id"
 AND a."wID" = m."wMonAttr" AND a."bLevel" = m."bLevel"
WHERE a."_source_row_number" IS NULL;
COMMENT ON VIEW runtime_control.missing_monster_attributes IS
  'Source catalog gaps retained for diagnosis; no placeholder combat values are introduced';
REVOKE ALL ON ALL TABLES IN SCHEMA game_compat, runtime_control FROM PUBLIC;
