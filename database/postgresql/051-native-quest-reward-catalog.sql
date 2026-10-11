-- Additive publication of quest reward definitions and the original level cap.
-- Existing condition-only publications remain usable for conditions, but do
-- not authorize rewards until the extended archive has been explicitly checked.
ALTER TABLE runtime_control.quest_condition_catalog
    ADD COLUMN character_manifest text CHECK (character_manifest ~ '^[0-9a-f]{64}$'),
    ADD COLUMN actor_manifest text CHECK (actor_manifest ~ '^[0-9a-f]{64}$');
ALTER TABLE runtime_control.quest_condition_activations
    ADD COLUMN character_manifest text CHECK (character_manifest ~ '^[0-9a-f]{64}$'),
    ADD COLUMN actor_manifest text CHECK (actor_manifest ~ '^[0-9a-f]{64}$');
CREATE VIEW game_compat.quest_reward_release AS
    SELECT a.run_id,r.manifest_sha256,r.status,a.map_manifest,a.character_manifest,a.actor_manifest
    FROM runtime_control.quest_condition_catalog a JOIN reconstruction.import_runs r ON r.id=a.run_id
    WHERE a.singleton AND a.character_manifest IS NOT NULL AND a.actor_manifest IS NOT NULL;
CREATE VIEW game_compat."TQUESTITEMCHART" AS
    SELECT s.* FROM legacy_game."TQUESTITEMCHART" s JOIN game_compat.quest_reward_release a
        ON a.run_id=s."_import_run_id";
CREATE VIEW game_compat."TLIMITEDLEVELCHART" AS
    SELECT s."bMaxLevel",s."bNation" FROM legacy_global."TLIMITEDLEVELCHART" s
        JOIN game_compat.quest_reward_release a ON a.run_id=s."_import_run_id";
CREATE VIEW game_compat."TTITLECHART" AS
    SELECT s."wTitleID",s."bKind",s."dwRequirement" FROM legacy_game."TTITLECHART" s
        JOIN game_compat.quest_reward_release a ON a.run_id=s."_import_run_id";
CREATE VIEW game_compat."TPORTALCHART" AS
    SELECT s.* FROM legacy_game."TPORTALCHART" s JOIN game_compat.quest_reward_release a ON a.run_id=s."_import_run_id";
CREATE VIEW game_compat."TDESTINATIONCHART" AS
    SELECT s.* FROM legacy_game."TDESTINATIONCHART" s JOIN game_compat.quest_reward_release a ON a.run_id=s."_import_run_id";
COMMENT ON VIEW game_compat."TLIMITEDLEVELCHART" IS
    'TGetLimitedLevel reads TGLOBAL_GSP.dbo.TLIMITEDLEVELCHART. Activation requires an explicit acknowledgement mapping that source database name to the pinned TGLOBAL_RAGEZONE backup; no level cap is invented.';
REVOKE ALL ON game_compat.quest_reward_release,game_compat."TQUESTITEMCHART",
    game_compat."TLIMITEDLEVELCHART",game_compat."TTITLECHART",
    game_compat."TPORTALCHART",game_compat."TDESTINATIONCHART" FROM PUBLIC;
