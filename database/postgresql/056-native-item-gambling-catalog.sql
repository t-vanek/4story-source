-- Existing content receipts do not certify the complete TGAMBLECHART. An
-- explicit offline publication is required; no historical values are changed.
ALTER TABLE runtime_control.quest_condition_catalog
    ADD COLUMN item_gambling boolean NOT NULL DEFAULT false;
ALTER TABLE runtime_control.quest_condition_activations
    ADD COLUMN item_gambling boolean NOT NULL DEFAULT false;
CREATE VIEW game_compat.item_gambling_release AS
    SELECT r.* FROM game_compat.quest_reward_release r
        JOIN runtime_control.quest_condition_catalog c ON c.run_id=r.run_id
        WHERE c.singleton AND c.item_gambling;
CREATE VIEW game_compat."TGAMBLECHART" AS
    SELECT s.* FROM legacy_game."TGAMBLECHART" s
        JOIN game_compat.item_gambling_release r ON r.run_id=s."_import_run_id";
CREATE VIEW game_compat.item_gambling_magic AS
    SELECT s.* FROM legacy_game."TITEMMAGICCHART" s
        JOIN game_compat.item_gambling_release r ON r.run_id=s."_import_run_id";
COMMENT ON VIEW game_compat."TGAMBLECHART" IS
    'Complete CTBLGamble source rows. Runtime uses explicit archive extract order for the original unordered SELECT, WORD probability accumulation, and original unfiltered min/max levels. Empty groups are meaningful only after full publication.';
COMMENT ON VIEW game_compat.item_gambling_magic IS
    'Source CTBLItemMagicChart with archive row order, identical to the active actor catalog. Preserves the original LoadData zero-initialized but unassigned bMinLevel and four-argument MakeSpecialItem call semantics; archival data remains unchanged.';
REVOKE ALL ON game_compat.item_gambling_release,game_compat."TGAMBLECHART",game_compat.item_gambling_magic FROM PUBLIC;
