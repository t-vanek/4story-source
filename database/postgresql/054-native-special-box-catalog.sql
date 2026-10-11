-- Additive publication gate. Old quest catalog receipts do not certify that
-- TSPECIALBOXCHART was inspected; preserve them until explicit offline upgrade.
ALTER TABLE runtime_control.quest_condition_catalog
    ADD COLUMN special_boxes boolean NOT NULL DEFAULT false;
ALTER TABLE runtime_control.quest_condition_activations
    ADD COLUMN special_boxes boolean NOT NULL DEFAULT false;
CREATE VIEW game_compat.special_box_release AS
    SELECT r.* FROM game_compat.quest_reward_release r
        JOIN runtime_control.quest_condition_catalog c ON c.run_id=r.run_id
        WHERE c.singleton AND c.special_boxes;
CREATE VIEW game_compat."TSPECIALBOXCHART" AS
    SELECT s.* FROM legacy_game."TSPECIALBOXCHART" s
        JOIN game_compat.special_box_release r ON r.run_id=s."_import_run_id";
COMMENT ON VIEW game_compat."TSPECIALBOXCHART" IS
    'Original CTBLSpecialBoxChart rows, including original text bClass and signed storage bits. Runtime uses archive row order explicitly because the original SELECT has no ORDER BY. No synthetic box contents.';
REVOKE ALL ON game_compat.special_box_release,game_compat."TSPECIALBOXCHART" FROM PUBLIC;
