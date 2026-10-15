-- Written only: apply after072 with the existing native Map runtime grants.
BEGIN;
GRANT USAGE ON SCHEMA skill_context_compat TO :"map_role";
GRANT SELECT ON skill_context_compat.catalog_release,
    skill_context_compat."TGUILDSKILLCHART" TO :"map_role";
COMMIT;
