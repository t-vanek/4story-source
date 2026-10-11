-- Run as owner with psql -v patch_role=DEDICATED_PATCH_ROLE.
BEGIN;
GRANT USAGE ON SCHEMA app_patch TO :"patch_role";
GRANT SELECT ON ALL TABLES IN SCHEMA app_patch TO :"patch_role";
COMMIT;
-- No promotion/write grant: the source TPreCompleteAdd body is unavailable.
