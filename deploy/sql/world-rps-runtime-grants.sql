-- Phase A: written only; use after074 and the existing World/social grants.
BEGIN;
GRANT USAGE ON SCHEMA world_compat TO :"world_role";
GRANT SELECT ON world_compat.rps_release,world_compat."TRPSGAMECHART" TO :"world_role";
GRANT SELECT,INSERT,DELETE ON app_world.rps_records TO :"world_role";
GRANT SELECT,INSERT ON app_world.rps_operations TO :"world_role";
GRANT EXECUTE ON FUNCTION app_world.rps_source_minute(bigint,text) TO :"world_role";
GRANT USAGE,SELECT ON SEQUENCE app_world.rps_records_record_id_seq,
    app_world.rps_operations_operation_id_seq TO :"world_role";
COMMIT;
