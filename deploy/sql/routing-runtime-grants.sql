-- Apply after login-runtime-grants.sql and character-runtime-grants.sql.
BEGIN;
GRANT USAGE ON SCHEMA route_compat TO :"login_role";
GRANT SELECT ON ALL TABLES IN SCHEMA route_compat TO :"login_role";
GRANT SELECT ON app_world.routing_worlds TO :"login_role";
GRANT INSERT,UPDATE,DELETE ON app_global.map_handoff TO :"login_role";
COMMIT;
