-- Incremental native social grants after001-066; other World repositories
-- still need their own native ports. Run only in Phase B/C, with psql variables.
BEGIN;
GRANT USAGE ON SCHEMA app_world,app_global TO :"world_role";
GRANT SELECT,INSERT,UPDATE ON app_world.world_runtime_owner TO :"world_role";
GRANT SELECT,INSERT,UPDATE,DELETE ON app_world."TFRIENDGROUPTABLE",app_world."TFRIENDTABLE",app_world."TSOULMATETABLE" TO :"world_role";
GRANT SELECT,INSERT ON app_world.social_mutations TO :"world_role";
GRANT USAGE ON SEQUENCE app_world.social_mutations_mutation_id_seq TO :"world_role";
GRANT SELECT ON app_world."TCHARTABLE",app_world."TPROTECTEDTABLE",app_world.map_sessions,
    app_world.map_runtime_owner,app_global."TCURRENTUSER" TO :"world_role";
-- Basic native party DTOs require authoritative guildlessness; absence of
-- hydrated guild data cannot stand in for this membership proof.
GRANT SELECT ON app_world.guild_membership TO :"world_role";
-- Column-scoped UPDATE only supplies PostgreSQL FOR SHARE privileges. No
-- social statement changes these identity/Map/account columns.
GRANT SELECT(user_id,disabled,unlocked_key) ON app_global.secure_codes TO :"world_role";
GRANT UPDATE("dwCharID") ON app_world."TCHARTABLE",app_global."TCURRENTUSER" TO :"world_role";
GRANT SELECT("dwUserID"),UPDATE("dLastLogin") ON app_global."TACCOUNT_PW" TO :"world_role";
GRANT UPDATE(char_id) ON app_world.map_sessions TO :"world_role";
GRANT UPDATE(owner_token) ON app_world.map_runtime_owner TO :"world_role";
COMMIT;
