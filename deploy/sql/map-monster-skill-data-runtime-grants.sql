-- Phase B only; written, not executed. Requires083 and existing native Map grants.
BEGIN;
GRANT SELECT ON monster_compat.skill_data_release,monster_compat.skill_data_queries,
    monster_compat.skill_data,monster_compat.premium_skill_removals TO :"map_role";
COMMIT;
