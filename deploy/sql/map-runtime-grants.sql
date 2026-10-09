-- psql -v map_role=dedicated_map_login; never grant historical writes.
BEGIN;
ALTER ROLE :"map_role" SET search_path=pg_catalog;
GRANT USAGE ON SCHEMA app_global,app_world,character_compat,route_compat,actor_compat TO :"map_role";
GRANT SELECT ON ALL TABLES IN SCHEMA character_compat,route_compat,actor_compat TO :"map_role";
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
GRANT UPDATE("dwRemainTick") ON app_world."TSKILLTABLE" TO :"map_role";
GRANT UPDATE("bLevel","dwEXP","dwHP","dwMP","dwGold","dwSilver","dwCooper","wSkillPoint","dwRegion",
    "wMapID","wSpawnID","wLastSpawnID","dwLastDestination","wTemptedMon","bAftermath","bStartAct",
    "fPosX","fPosY","fPosZ","wDIR","bStatLevel","bStatPoint","dwStatExp","dLogoutDate")
    ON app_world."TCHARTABLE" TO :"map_role";
COMMIT;
