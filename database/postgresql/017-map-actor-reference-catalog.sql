-- Independent backup-derived item magic and skill-point metadata for Map loading.
CREATE SCHEMA actor_compat;
CREATE TABLE runtime_control.actor_catalog (
 singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
 run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
 activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE runtime_control.actor_catalog_activations (
 id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 previous_run_id bigint REFERENCES reconstruction.import_runs(id),
 run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
 manifest_sha256 text NOT NULL,
 activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE VIEW actor_compat.catalog_release AS
 SELECT a.run_id,r.manifest_sha256,r.status FROM runtime_control.actor_catalog a
 JOIN reconstruction.import_runs r ON r.id=a.run_id WHERE a.singleton;
CREATE VIEW actor_compat."TITEMMAGICCHART" AS
SELECT s."_import_run_id" AS release_id, s."bMagic", s."dwKind", s."bRvType", s."wMaxValue", s."bIsMagic", s."bIsRare", s."bMinLevel", s."bExclIndex", s."bOptionKind", s."wAutoSkill", s."bRefine", s."wMaxBound", s."wRareBound"
FROM legacy_game."TITEMMAGICCHART" s
JOIN runtime_control.actor_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW actor_compat."TSKILLPOINTCHART" AS
SELECT s."_import_run_id" AS release_id, s."wID", s."bLevel", s."bSkillPoint", s."bGroupPoint", s."bPrevSkillLevel", s."dwPayback"
FROM legacy_game."TSKILLPOINTCHART" s
JOIN runtime_control.actor_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
REVOKE ALL ON SCHEMA actor_compat FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA actor_compat FROM PUBLIC;
REVOKE ALL ON runtime_control.actor_catalog,runtime_control.actor_catalog_activations FROM PUBLIC;

-- Already part of the verified 22-table character snapshot.
CREATE VIEW character_compat."TFORMULACHART" AS
SELECT s."_import_run_id" AS release_id, s."bID", s."szName", s."dwinit", s."fRateX", s."fRateY"
FROM legacy_game."TFORMULACHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";

-- Already part of the verified 22-table character snapshot.
CREATE VIEW character_compat."TSKILLCHART" AS
SELECT s."_import_run_id" AS release_id, s."wID", s."szName", s."wPrevActiveID", s."wParentSkillID", s."wItemID", s."wMaxRange", s."wMinRange", s."wPosture", s."dwConditionID", s."dwWeaponID", s."dwClassID", s."bKind", s."fPrice", s."dwUseMP", s."bUseMPType", s."dwUseHP", s."bUseHPType", s."dwReuseDelay", s."nReuseDelayInc", s."dwLoopDelay", s."dwActionTime", s."dwDuration", s."dwDurationInc", s."dwKindDelay", s."dwAggro", s."dwAggroInc", s."bLevel", s."bMaxLevel", s."bNextLevel", s."bTarget", s."bTargetRange", s."bIsuse", s."bTargetHit", s."bPositive", s."bPriority", s."bSpeedApply", s."bCanLearn", s."bORadius", s."bIsRide", s."bIsDismount", s."wTargetActiveID", s."bMaintainType", s."bDuraSlot", s."bCanCancel", s."bHitTest", s."bHitInit", s."bHitInc", s."bGlobal", s."bRadius", s."bStatic", s."bEraseAct", s."bEraseHide", s."bIsHideSkill", s."bRunFromServer", s."bCheckAttacker", s."wTriggerID", s."wMapID", s."bRepeatCount"
FROM legacy_game."TSKILLCHART" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";

-- Already part of the verified 22-table character snapshot.
CREATE VIEW character_compat."TSKILLDATA" AS
SELECT s."_import_run_id" AS release_id, s."wSkillID", s."bAction", s."bType", s."bAttr", s."bExec", s."bInc", s."wValue", s."wValueInc", s."bCalc"
FROM legacy_game."TSKILLDATA" s
JOIN runtime_control.character_catalog a ON a.singleton AND a.run_id=s."_import_run_id";

REVOKE ALL ON ALL TABLES IN SCHEMA character_compat FROM PUBLIC;
