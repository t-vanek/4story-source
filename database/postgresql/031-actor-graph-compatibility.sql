-- Explicit, directional evidence for the additive two -> four table actor
-- upgrade. Original imports, player bodies and applied migrations stay intact.
CREATE TABLE runtime_control.actor_graph_compatibility (
 source_run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
 target_run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
 source_manifest_sha256 text NOT NULL CHECK(source_manifest_sha256 ~ '^[0-9a-f]{64}$'),
 target_manifest_sha256 text NOT NULL CHECK(target_manifest_sha256 ~ '^[0-9a-f]{64}$'),
 policy text NOT NULL CHECK(policy='actor-v1-to-v2-additive-statistics'),
 proof jsonb NOT NULL CHECK(jsonb_typeof(proof)='object'),
 certified_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 certified_by text NOT NULL DEFAULT session_user,
 PRIMARY KEY(source_run_id,target_run_id),
 UNIQUE(source_manifest_sha256,target_manifest_sha256),
 CHECK(source_run_id<>target_run_id),
 CHECK(source_manifest_sha256<>target_manifest_sha256)
);
COMMENT ON TABLE runtime_control.actor_graph_compatibility IS
 'Schema-owner evidence only. The publisher verifies both complete imports, original bytes, pinned backup provenance and unchanged old charts in one transaction before certifying the additive v1-to-v2 upgrade. This is not transitive, not permission to change gameplay data, and never rewrites a player receipt. Map runtime can read only the current-target projection.';
REVOKE ALL ON runtime_control.actor_graph_compatibility FROM PUBLIC;

CREATE VIEW actor_compat.transfer_catalog_compatibility AS
 SELECT p.source_manifest_sha256,p.target_manifest_sha256
 FROM runtime_control.actor_graph_compatibility p
 JOIN reconstruction.import_runs s ON s.id=p.source_run_id
   AND s.manifest_sha256=p.source_manifest_sha256 AND s.status='verified'
 JOIN reconstruction.import_runs t ON t.id=p.target_run_id
   AND t.manifest_sha256=p.target_manifest_sha256 AND t.status='verified'
 JOIN runtime_control.actor_catalog a ON a.singleton AND a.run_id=t.id
 WHERE p.policy='actor-v1-to-v2-additive-statistics';
REVOKE ALL ON actor_compat.transfer_catalog_compatibility FROM PUBLIC;

CREATE TABLE runtime_control.actor_owner_retirements (
 activation_id bigint NOT NULL REFERENCES runtime_control.actor_catalog_activations(id),
 world_id smallint NOT NULL,
 server_id smallint NOT NULL,
 previous_token text NOT NULL CHECK(previous_token ~ '^[0-9a-f]{64}$'),
 replacement_token text NOT NULL CHECK(replacement_token ~ '^[0-9a-f]{64}$'),
 PRIMARY KEY(activation_id,world_id,server_id),
 CHECK(previous_token<>replacement_token)
);
COMMENT ON TABLE runtime_control.actor_owner_retirements IS
 'Offline actor publication fences any detached old Map worker by rotating inactive owner tokens while holding the owner table exclusively. Session/checkpoint/journal tokens remain unchanged for the ordinary verified process-replacement recovery path.';
REVOKE ALL ON runtime_control.actor_owner_retirements FROM PUBLIC;
