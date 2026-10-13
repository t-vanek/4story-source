-- Written only: apply after068 in Phase B/C with existing World runtime grants.
BEGIN;
GRANT USAGE ON SCHEMA world_compat TO :"world_role";
GRANT SELECT ON world_compat.guild_level_release,world_compat."TGUILDCHART" TO :"world_role";
COMMIT;
