-- Phase B only, written but NOT executed. Migration080; explicit map_role.
BEGIN;
GRANT SELECT ON monster_compat.ai_release,monster_compat."TAICMDCHART",
    monster_compat."TAICONCHART",monster_compat."TAICHART" TO :"map_role";
COMMIT;
