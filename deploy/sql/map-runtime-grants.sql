-- psql -v map_role=dedicated_map_login; never grant historical writes.
BEGIN;
ALTER ROLE :"map_role" SET search_path=pg_catalog;
GRANT USAGE ON SCHEMA app_global,app_world,character_compat,route_compat,actor_compat,game_compat TO :"map_role";
GRANT SELECT ON ALL TABLES IN SCHEMA character_compat,route_compat,actor_compat,game_compat TO :"map_role";
GRANT SELECT("dwUserID") ON app_global."TACCOUNT_PW" TO :"map_role";
GRANT SELECT ON app_global."TCURRENTUSER",app_global."TUSERINFOTABLE",
    app_global."TALLCHARTABLE",app_global."TLOG",app_global.map_handoff,
    app_global."TSERVER",app_global."TMACHINE",app_global."TIPADDR" TO :"map_role";
GRANT UPDATE("dLastLogin") ON app_global."TACCOUNT_PW" TO :"map_role";
GRANT UPDATE("bLocked"),DELETE ON app_global."TCURRENTUSER" TO :"map_role";
GRANT UPDATE("timeLOGOUT") ON app_global."TLOG" TO :"map_role";
GRANT UPDATE("expires_at"),DELETE ON app_global.map_handoff TO :"map_role";
GRANT UPDATE("bRouteID") ON app_global."TMACHINE" TO :"map_role";
GRANT SELECT ON ALL TABLES IN SCHEMA app_world TO :"map_role";
GRANT INSERT,UPDATE,DELETE ON app_world.map_sessions TO :"map_role";
GRANT INSERT,UPDATE,DELETE ON app_world.map_replicas TO :"map_role";
GRANT INSERT,UPDATE ON app_world.map_runtime_owner TO :"map_role";
GRANT INSERT,UPDATE ON app_world.map_transfers TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.map_transfers_transfer_id_seq TO :"map_role";
GRANT INSERT,UPDATE ON app_world.map_checkpoints TO :"map_role";
GRANT EXECUTE ON FUNCTION app_world.map_core_state(smallint,integer) TO :"map_role";
GRANT EXECUTE ON FUNCTION app_world.map_skill_state(smallint,integer),app_world.map_checkpoint_matches(app_world.map_checkpoints) TO :"map_role";
GRANT INSERT,UPDATE("dwRemainTick","bLevel") ON app_world."TSKILLTABLE" TO :"map_role";
GRANT INSERT,DELETE ON app_world.map_maintained_effects TO :"map_role";
GRANT INSERT ON app_world.maintained_effect_operations TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.maintained_effect_operations_operation_id_seq TO :"map_role";
GRANT EXECUTE ON FUNCTION app_world.map_maintain_state(smallint,integer) TO :"map_role";
GRANT EXECUTE ON FUNCTION app_world.item_fingerprint(app_world."TITEMTABLE") TO :"map_role";
GRANT UPDATE("bCount"),DELETE ON app_world."TITEMTABLE" TO :"map_role";
GRANT UPDATE("dwStorageID","bItemID") ON app_world."TITEMTABLE" TO :"map_role";
GRANT UPDATE("bStorageType",post_id) ON app_world."TITEMTABLE" TO :"map_role";
GRANT DELETE ON app_world."TINVENTABLE" TO :"map_role";
GRANT INSERT,DELETE,UPDATE("bRead","dwGold","dwSilver","dwCooper") ON app_world."TPOSTTABLE" TO :"map_role";
GRANT USAGE ON SEQUENCE app_world."TPOSTTABLE_dwPostID_seq" TO :"map_role";
GRANT UPDATE("dwTime2","wMoggItemID") ON app_world."TITEMTABLE" TO :"map_role";
GRANT UPDATE("bLevel","bGem","bGradeEffect","dwTime1","dwTime3") ON app_world."TITEMTABLE" TO :"map_role";
GRANT UPDATE("dwDuraMax","dwDuraCur","bRefineCur",
    "bMagic1","wValue1","bMagic2","wValue2","bMagic3","wValue3",
    "bMagic4","wValue4","bMagic5","wValue5","bMagic6","wValue6")
    ON app_world."TITEMTABLE" TO :"map_role";
GRANT INSERT ON app_world.inventory_movements TO :"map_role";
GRANT INSERT ON app_world.equipment_operations,app_world.equipment_item_changes TO :"map_role";
GRANT INSERT ON app_world.inventory_stack_changes,app_world."TITEMTABLE" TO :"map_role";
GRANT UPDATE(item_high_water) ON app_world.worlds TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.inventory_movements_movement_id_seq TO :"map_role";
GRANT INSERT ON app_world.accepted_skill_casts TO :"map_role";
GRANT INSERT ON app_world.skill_finish_operations TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.skill_finish_operations_finish_id_seq TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.accepted_skill_casts_cast_id_seq TO :"map_role";
GRANT INSERT ON app_world.skill_item_consumptions TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.skill_item_consumptions_consumption_id_seq TO :"map_role";
GRANT UPDATE("bLevel","dwEXP","dwHP","dwMP","dwGold","dwSilver","dwCooper","wSkillPoint","dwRegion",
    "wMapID","wSpawnID","wLastSpawnID","dwLastDestination","wTemptedMon","bAftermath","bStartAct",
    "fPosX","fPosY","fPosZ","wDIR","bStatLevel","bStatPoint","dwStatExp","dLogoutDate")
    ON app_world."TCHARTABLE" TO :"map_role";
GRANT INSERT ON app_world.skill_action_operations TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.skill_action_operations_action_id_seq TO :"map_role";

GRANT INSERT ON app_world.title_selection_operations TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.title_selection_operations_selection_id_seq TO :"map_role";
GRANT INSERT ON app_world.skill_purchase_operations TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.skill_purchase_operations_purchase_id_seq TO :"map_role";
GRANT INSERT ON app_world."TSKILLLOG" TO :"map_role";
GRANT USAGE ON SEQUENCE app_world."TSKILLLOG_log_id_seq" TO :"map_role";
GRANT INSERT,UPDATE ON app_world."TQUESTTABLE",app_world."TQUESTTERMTABLE" TO :"map_role";
GRANT INSERT ON app_world.quest_operations TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.quest_operations_operation_id_seq TO :"map_role";
GRANT EXECUTE ON FUNCTION app_world.map_quest_state(smallint,integer) TO :"map_role";
GRANT INSERT,DELETE,UPDATE("bSelected") ON app_world."TTITLETABLE" TO :"map_role";
GRANT SELECT,INSERT,UPDATE ON app_global.secure_codes TO :"map_role";
GRANT SELECT ON app_global.during_items TO :"map_role";
GRANT INSERT,DELETE ON app_world.exp_entitlements TO :"map_role";
GRANT EXECUTE ON FUNCTION app_world.map_exp_state(smallint,integer) TO :"map_role";
GRANT INSERT,DELETE ON app_world.item_cooldowns TO :"map_role";
GRANT UPDATE(available) ON app_world.pvp_balances TO :"map_role";
GRANT EXECUTE ON FUNCTION app_world.map_item_cooldown_state(smallint,integer) TO :"map_role";
GRANT INSERT ON app_world.secure_code_operations TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.secure_code_operations_operation_id_seq TO :"map_role";
COMMIT;
