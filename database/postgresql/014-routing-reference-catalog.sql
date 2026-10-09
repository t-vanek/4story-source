-- Backup-derived routing publication; independent of Map and character releases.
CREATE SCHEMA route_compat;
CREATE TABLE runtime_control.routing_catalog (
    singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE runtime_control.routing_catalog_activations (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    previous_run_id bigint REFERENCES reconstruction.import_runs(id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL,
    activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE VIEW route_compat.catalog_release AS
SELECT a.run_id,r.manifest_sha256,r.status FROM runtime_control.routing_catalog a
JOIN reconstruction.import_runs r ON r.id=a.run_id WHERE a.singleton;
CREATE VIEW route_compat."TCHANNELCHART" AS
SELECT s."_import_run_id" AS release_id, s."bGroupID", s."wMapID", s."wUnitID", s."bLogChannel", s."bPhyChannel"
FROM legacy_game."TCHANNELCHART" s
JOIN runtime_control.routing_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW route_compat."TSPAWNPOSCHART" AS
SELECT s."_import_run_id" AS release_id, s."wID", s."wMapID", s."fPosX", s."fPosY", s."fPosZ", s."bType"
FROM legacy_game."TSPAWNPOSCHART" s
JOIN runtime_control.routing_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW route_compat."TMAPCHART" AS
SELECT s."_import_run_id" AS release_id, s."bGroupID", s."wMapID", s."bServerID", s."bChannel"
FROM legacy_game."TMAPCHART" s
JOIN runtime_control.routing_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW route_compat."TUNITCHART" AS
SELECT s."_import_run_id" AS release_id, s."bGroup", s."bServerID", s."wMapID", s."wUnitID"
FROM legacy_game."TUNITCHART" s
JOIN runtime_control.routing_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
-- TSVRCHART is a VIEW in the backup, not a physical table. Preserve its exact join.
CREATE VIEW route_compat."TSVRCHART" AS
SELECT u.release_id,u."bGroup",u."bServerID",u."wMapID",u."wUnitID",m."bChannel"
FROM route_compat."TUNITCHART" u JOIN route_compat."TMAPCHART" m
 ON u."wMapID"=m."wMapID" AND u."bServerID"=m."bServerID" AND u."bGroup"=m."bGroupID";
REVOKE ALL ON SCHEMA route_compat FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA route_compat FROM PUBLIC;
REVOKE ALL ON runtime_control.routing_catalog,runtime_control.routing_catalog_activations FROM PUBLIC;
