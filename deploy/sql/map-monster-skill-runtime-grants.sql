-- Phase B only; written, not executed. Requires082 and existing native Map grants.
BEGIN;
GRANT SELECT ON monster_compat.skill_release,monster_compat."TSKILLCHART",monster_compat."TFORMULACHART",
    monster_compat.skill_class_stats,monster_compat.skill_race_stats TO :"map_role";
COMMIT;
