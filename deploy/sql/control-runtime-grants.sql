-- Apply as schema owner after migrations 001–050; role is provisioned separately.
-- psql -v control_role=DEDICATED_CONTROL_ROLE -f deploy/sql/control-runtime-grants.sql
BEGIN;
GRANT USAGE ON SCHEMA app_control,app_global,app_world,app_patch,runtime_control TO :"control_role";
GRANT SELECT ON ALL TABLES IN SCHEMA app_control TO :"control_role";
GRANT INSERT,UPDATE ON app_control.writer TO :"control_role";
GRANT INSERT,UPDATE,DELETE ON app_control."TEVENTCHART" TO :"control_role";
GRANT SELECT ON app_global."TACCOUNT_PW",app_world."TCHARTABLE" TO :"control_role";
-- PostgreSQL also requires an UPDATE privilege for the row ownership locks.
GRANT UPDATE ("dwUserID") ON app_global."TACCOUNT_PW",app_world."TCHARTABLE" TO :"control_role";
GRANT SELECT,INSERT ON app_global."TUSERPROTECTED" TO :"control_role";
GRANT USAGE ON SEQUENCE app_global."TUSERPROTECTED_dwSeq_seq" TO :"control_role";
GRANT SELECT ON ALL TABLES IN SCHEMA app_patch TO :"control_role";
GRANT INSERT,UPDATE ON app_patch."TVERSION" TO :"control_role";
GRANT SELECT,INSERT,UPDATE,DELETE ON runtime_control.peer_registry TO :"control_role";
GRANT SELECT,INSERT,DELETE ON runtime_control.peer_status_log,runtime_control.peer_metrics TO :"control_role";
GRANT INSERT ON runtime_control.operator_audit TO :"control_role";
GRANT USAGE ON SEQUENCE runtime_control.peer_epoch,runtime_control.peer_status_log_id_seq,
    runtime_control.peer_metrics_id_seq,runtime_control.operator_audit_id_seq TO :"control_role";
COMMIT;
-- Optional trust-store grants are in peer-security-runtime-grants.sql. No archive
-- access, operator credential management, SMS or missing pre-patch write rights.
