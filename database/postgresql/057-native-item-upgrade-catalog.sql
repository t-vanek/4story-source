-- Additive source publication. Neither the previous quest nor gambling profile
-- establishes complete gem/accessory definitions. Keep every old receipt intact.
ALTER TABLE runtime_control.quest_condition_catalog
    ADD COLUMN item_upgrades boolean NOT NULL DEFAULT false;
ALTER TABLE runtime_control.quest_condition_activations
    ADD COLUMN item_upgrades boolean NOT NULL DEFAULT false;
CREATE VIEW game_compat.item_upgrade_release AS
    SELECT r.* FROM game_compat.quest_reward_release r
        JOIN runtime_control.quest_condition_catalog c ON c.run_id=r.run_id
        WHERE c.singleton AND c.item_upgrades;
CREATE VIEW game_compat."TGEMGRADECHART" AS
    SELECT s.* FROM legacy_game."TGEMGRADECHART" s
        JOIN game_compat.item_upgrade_release r ON r.run_id=s."_import_run_id";
CREATE VIEW game_compat."ACCESSORYMAGICTABLE" AS
    SELECT s.* FROM legacy_game."ACCESSORYMAGICTABLE" s
        JOIN game_compat.item_upgrade_release r ON r.run_id=s."_import_run_id";
CREATE VIEW game_compat.item_upgrade_magic AS
    SELECT s.* FROM legacy_game."TITEMMAGICCHART" s
        JOIN game_compat.item_upgrade_release r ON r.run_id=s."_import_run_id";
COMMENT ON VIEW game_compat."ACCESSORYMAGICTABLE" IS
    'Complete CTBLAccessoryMagic source rows, ordered by archive extract ordinal at runtime. BYTE/WORD bindings are explicit. Source MinValue cancels algebraically in the value draw; no replacement range or contents are invented.';
REVOKE ALL ON game_compat.item_upgrade_release,game_compat."TGEMGRADECHART",
    game_compat."ACCESSORYMAGICTABLE",game_compat.item_upgrade_magic FROM PUBLIC;
