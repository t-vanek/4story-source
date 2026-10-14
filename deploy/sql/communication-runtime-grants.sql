-- Written069/native communication permissions; isolated later acceptance only.
-- Existing World/Map social/runtime grants remain prerequisites.
BEGIN;
GRANT USAGE ON SCHEMA world_compat TO :"map_role",:"world_role";
GRANT SELECT ON world_compat.operator_release,app_world."TOPERATORTABLE",app_world.chat_bans
    TO :"map_role",:"world_role";
GRANT SELECT ON app_world.chat_ban_mutations TO :"map_role",:"world_role";
GRANT INSERT,UPDATE ON app_world.chat_bans TO :"world_role";
GRANT INSERT ON app_world.chat_ban_mutations TO :"world_role";
GRANT USAGE ON SEQUENCE app_world.chat_ban_mutations_mutation_id_seq TO :"world_role";
COMMIT;
