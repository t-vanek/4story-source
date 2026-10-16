-- Phase B only, written but NOT executed. Migration079; explicit map_role.
BEGIN;
GRANT USAGE ON SCHEMA monster_compat TO :"map_role";
GRANT SELECT ON monster_compat.catalog_release,monster_compat.allocation_counts,
    monster_compat.source_server_spawns,monster_compat."TMONSPAWNCHART",
    monster_compat."TMAPMONCHART",monster_compat."TMONPARTY",monster_compat."TMONSTERCHART",
    monster_compat."TMONATTRCHART",monster_compat."TCLASSCHART",monster_compat."TRACECHART",
    monster_compat."TLEVELCHART" TO :"map_role";
COMMIT;
