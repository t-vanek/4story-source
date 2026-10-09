-- Run as schema owner with psql -v log_role=YOUR_DEDICATED_ROLE.
-- Create LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE NOINHERIT and grant CONNECT
-- separately. This script creates no role/secret. No update/delete/DDL grants.
BEGIN;
GRANT USAGE ON SCHEMA app_audit TO :"log_role";
GRANT SELECT ON app_audit.runtime_contract,app_audit."TLOG_AUDIT" TO :"log_role";
GRANT INSERT (lt_logdate,lt_serverid,lt_clientip,lt_action,lt_mapid,lt_x,lt_y,lt_z,lt_dwkey1,lt_dwkey2,lt_dwkey3,lt_dwkey4,lt_dwkey5,lt_dwkey6,lt_dwkey7,lt_dwkey8,lt_dwkey9,lt_dwkey10,lt_dwkey11,lt_key1,lt_key2,lt_key3,lt_key4,lt_key5,lt_key6,lt_key7,lt_fmt,lt_log) ON app_audit."TLOG_AUDIT" TO :"log_role";
GRANT USAGE ON SEQUENCE app_audit."TLOG_AUDIT_lt_id_seq" TO :"log_role";
COMMIT;
