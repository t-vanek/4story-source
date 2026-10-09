-- Run after login-runtime-grants.sql; credentials/role creation stay external.
BEGIN;
GRANT USAGE ON SCHEMA app_world, character_compat TO :"login_role";
GRANT SELECT ON ALL TABLES IN SCHEMA character_compat, app_world TO :"login_role";
GRANT UPDATE (item_high_water) ON app_world.worlds TO :"login_role";
GRANT INSERT, UPDATE, DELETE ON
    app_global."TALLCHARTABLE", app_world."TCHARTABLE", app_world."TINVENTABLE",
    app_world."TTITLETABLE", app_world."TCABINETTABLE", app_world."TSKILLTABLE",
    app_world."THOTKEYTABLE", app_world."TPOSTTABLE", app_world."TRECALLMONTABLE",
    app_world."TPETTABLE", app_world."TITEMTABLE" TO :"login_role";
GRANT USAGE ON ALL SEQUENCES IN SCHEMA app_world TO :"login_role";
COMMIT;
