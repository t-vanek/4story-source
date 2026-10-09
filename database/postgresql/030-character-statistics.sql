-- Extend the pinned actor release for source equipment statistics.
-- Republish a complete four-table actor manifest; old two-table releases are
-- retained for provenance but cannot satisfy the new Map runtime contract.

CREATE VIEW actor_compat."TITEMATTRCHART" AS
SELECT s."_import_run_id" AS release_id, s."wID", s."bKind", s."bGrade", s."wMinAP", s."wMaxAP", s."wDP", s."wMinMAP", s."wMaxMAP", s."wMDP", s."bBlockProb"
FROM legacy_game."TITEMATTRCHART" s
JOIN runtime_control.actor_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
REVOKE ALL ON actor_compat."TITEMATTRCHART" FROM PUBLIC;

CREATE VIEW actor_compat."TITEMGRADECHART" AS
SELECT s."_import_run_id" AS release_id, s."bLevel", s."bGrade", s."bProb", s."dwMoney"
FROM legacy_game."TITEMGRADECHART" s
JOIN runtime_control.actor_catalog a ON a.singleton AND a.run_id=s."_import_run_id";
REVOKE ALL ON actor_compat."TITEMGRADECHART" FROM PUBLIC;

-- Expose only completeness of this pinned release, never import metadata or
-- historical rows to the runtime role. An old two-table release fails closed.
CREATE VIEW actor_compat.statistics_release AS
SELECT a.run_id FROM actor_compat.catalog_release a
WHERE a.status='verified' AND 2=(
 SELECT count(*) FROM reconstruction.checkpoints cp WHERE cp.run_id=a.run_id
 AND cp.source_database='TGAME_RAGEZONE' AND cp.source_schema='dbo'
 AND cp.source_table IN ('TITEMATTRCHART','TITEMGRADECHART'));
REVOKE ALL ON actor_compat.statistics_release FROM PUBLIC;
