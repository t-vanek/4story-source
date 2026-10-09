-- Extend the existing one-item transaction to verified single-hit ammunition.
-- Applied migrations and historical source data are immutable.
ALTER TABLE app_world.skill_item_consumptions
 ADD COLUMN consumption_kind text NOT NULL DEFAULT 'reagent'
 CHECK(consumption_kind IN ('reagent','ammunition'));
COMMENT ON COLUMN app_world.skill_item_consumptions.consumption_kind IS
 'Pinned skill reagent or one unit of the first powered compatible weapon ammunition kind. Ammo supports exactly one non-expanded hit; weapon, item, core, cooldown and audit validation share one transaction. Cash templates absent from the backup are not invented.';
