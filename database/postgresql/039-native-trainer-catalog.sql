-- Publish the original NPC merchandise list independently of existing immutable
-- character/actor manifests. The same list identifies trainer skills by NPC type.
CREATE TABLE runtime_control.trainer_catalog (
 singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
 run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
 character_manifest text NOT NULL CHECK(character_manifest ~ '^[0-9a-f]{64}$'),
 actor_manifest text NOT NULL CHECK(actor_manifest ~ '^[0-9a-f]{64}$'),
 activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE runtime_control.trainer_catalog_activations (
 id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
 previous_run_id bigint REFERENCES reconstruction.import_runs(id),
 run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
 manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
 character_manifest text NOT NULL CHECK(character_manifest ~ '^[0-9a-f]{64}$'),
 actor_manifest text NOT NULL CHECK(actor_manifest ~ '^[0-9a-f]{64}$'),
 activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE VIEW character_compat.trainer_release AS
SELECT a.run_id,r.manifest_sha256,r.status,a.character_manifest,a.actor_manifest
FROM runtime_control.trainer_catalog a JOIN reconstruction.import_runs r ON r.id=a.run_id WHERE a.singleton;
CREATE VIEW character_compat."TNPCITEMCHART" AS
SELECT s."_import_run_id" AS release_id,s."wNpcID",s."dwItemID"
FROM legacy_game."TNPCITEMCHART" s
JOIN runtime_control.trainer_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW character_compat."TTITLECHART" AS
SELECT s."_import_run_id" AS release_id,s."wTitleID",s."wCategory",s."strTitle",s."bKind",s."dwRequirement"
FROM legacy_game."TTITLECHART" s
JOIN runtime_control.trainer_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
REVOKE ALL ON character_compat."TTITLECHART" FROM PUBLIC;
CREATE TABLE runtime_control.trainer_owner_retirements (
 activation_id bigint NOT NULL REFERENCES runtime_control.trainer_catalog_activations(id),
 world_id smallint NOT NULL,
 server_id smallint NOT NULL,
 previous_token text NOT NULL,
 replacement_token text NOT NULL,
 PRIMARY KEY(activation_id,world_id,server_id)
);
REVOKE ALL ON runtime_control.trainer_catalog,runtime_control.trainer_catalog_activations FROM PUBLIC;
REVOKE ALL ON runtime_control.trainer_owner_retirements FROM PUBLIC;
REVOKE ALL ON character_compat.trainer_release,character_compat."TNPCITEMCHART" FROM PUBLIC;
COMMENT ON VIEW character_compat.trainer_release IS
 'Verified backup merchandise tied to the character NPC/skill and actor skill-point catalogs. Publication is offline; existing Map owners block it. Missing or mismatched publication cannot authorize native skill purchases.';
