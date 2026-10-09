-- Run as the application schema owner using psql -v login_role=YOUR_ROLE.
-- Provision a dedicated LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOINHERIT
-- role and its secret separately. This script creates no users or passwords.
-- Grant CONNECT on the chosen application database separately.
BEGIN;
ALTER ROLE :"login_role" SET search_path = app_global, pg_catalog;
GRANT USAGE ON SCHEMA app_global TO :"login_role";
GRANT SELECT ON ALL TABLES IN SCHEMA app_global TO :"login_role";
-- Account row locks require UPDATE privilege; credentials remain read-only.
GRANT UPDATE ("dLastLogin") ON app_global."TACCOUNT_PW" TO :"login_role";
GRANT INSERT, UPDATE, DELETE ON
    app_global."TCURRENTUSER", app_global."TLOG", app_global."USERIPLOG",
    app_global."TUSERINFOTABLE", app_global.login_security_challenge
    TO :"login_role";
GRANT USAGE ON SEQUENCE app_global."TCURRENTUSER_dwKEY_seq",
    app_global."USERIPLOG_id_seq" TO :"login_role";
GRANT UPDATE ("bRouteID") ON app_global."TMACHINE" TO :"login_role";
GRANT INSERT, UPDATE ON app_global.login_security_rate TO :"login_role";
GRANT INSERT, UPDATE ON app_global.login_runtime_owner TO :"login_role";
GRANT USAGE ON SCHEMA app_world TO :"login_role";
GRANT SELECT ON app_world.map_sessions TO :"login_role";
COMMIT;
