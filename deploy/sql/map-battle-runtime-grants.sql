-- Phase B only, after078 and explicit offline projection. No archive writes.
BEGIN;
GRANT USAGE ON SCHEMA battle_compat TO :"map_role";
GRANT SELECT ON battle_compat.catalog_release,battle_compat."TBATTLEZONECHART",battle_compat."TINDUNCHART",
    app_world.battle_regions,app_world.battle_occupation_days TO :"map_role";
COMMIT;
