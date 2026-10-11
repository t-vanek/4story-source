-- Quest conditions were absent from the original fifteen-table Map projection.
-- Publish them from an explicitly verified archive; never invent an empty list.
CREATE TABLE runtime_control.quest_condition_catalog (
    singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    map_manifest text NOT NULL CHECK (map_manifest ~ '^[0-9a-f]{64}$'),
    activated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE runtime_control.quest_condition_activations (
    activation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    previous_run_id bigint REFERENCES reconstruction.import_runs(id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    map_manifest text NOT NULL CHECK (map_manifest ~ '^[0-9a-f]{64}$'),
    activated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE VIEW game_compat.quest_condition_release AS
    SELECT a.run_id,r.manifest_sha256,r.status,a.map_manifest
    FROM runtime_control.quest_condition_catalog a JOIN reconstruction.import_runs r ON r.id=a.run_id WHERE a.singleton;
CREATE TABLE runtime_control.quest_condition_owner_retirements (
    activation_id bigint NOT NULL REFERENCES runtime_control.quest_condition_activations,
    world_id smallint NOT NULL, server_id smallint NOT NULL,
    previous_token text NOT NULL, replacement_token text NOT NULL,
    PRIMARY KEY (activation_id,world_id,server_id)
);
CREATE VIEW game_compat."TQCONDITIONCHART" AS
    SELECT s."dwID",s."dwQuestID",s."bConditionType",s."dwConditionID",s."bCount"
    FROM legacy_game."TQCONDITIONCHART" s JOIN runtime_control.quest_condition_catalog a
        ON a.singleton AND a.run_id=s."_import_run_id";
CREATE VIEW game_compat.tqconditionchart AS
    SELECT "dwID" AS dwid,"dwQuestID" AS dwquestid,"bConditionType" AS bconditiontype,
        "dwConditionID" AS dwconditionid,"bCount" AS bcount FROM game_compat."TQCONDITIONCHART";
REVOKE ALL ON runtime_control.quest_condition_catalog,runtime_control.quest_condition_activations,
    runtime_control.quest_condition_owner_retirements,
    game_compat.quest_condition_release,game_compat."TQCONDITIONCHART",game_compat.tqconditionchart FROM PUBLIC;
