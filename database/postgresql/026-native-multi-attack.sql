-- Source-generated multi-attack targets use the same atomic stack transaction.
-- Existing receipts retain their direct-target meaning; no historical edits.
ALTER TABLE app_world.skill_item_consumptions
 ADD COLUMN hit_mode text NOT NULL DEFAULT 'direct' CHECK(hit_mode IN ('direct','expanded')),
 ADD CONSTRAINT skill_consumption_expanded_ammunition CHECK(hit_mode='direct' OR consumption_kind='ammunition');
COMMENT ON COLUMN app_world.skill_item_consumptions.hit_count IS
 'Ammunition charge for 1..16 final server targets. Direct mode caps flagged inputs; expanded mode uses pinned MTYPE_EFC data and the authoritative learned rank. Reagent receipts retain one unit.';
COMMENT ON COLUMN app_world.skill_item_consumptions.hit_mode IS
 'Direct input targets or source multi-attack expansion. Expanded ammo requires the whole learned-rank hit budget; the server selects its target distribution. Original random seed sequences and budgets above MAX_TARGET are outside the current contract.';
