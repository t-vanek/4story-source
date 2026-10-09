-- One cast may consume several stacks in one source-ordered inventory.
-- Preserve historical data and applied migrations 001-024.
ALTER TABLE app_world.skill_item_consumptions
 DROP CONSTRAINT skill_item_consumptions_check,
 ADD COLUMN cast_id bigint,
 ADD COLUMN hit_count smallint NOT NULL DEFAULT 1 CHECK(hit_count BETWEEN 1 AND 16);
UPDATE app_world.skill_item_consumptions SET cast_id=consumption_id;
ALTER TABLE app_world.skill_item_consumptions
 ALTER COLUMN cast_id SET NOT NULL,
 ALTER COLUMN cast_id SET DEFAULT nextval('app_world.skill_item_consumptions_consumption_id_seq'),
 ADD CONSTRAINT skill_consumption_cast_positive CHECK(cast_id>0),
 ADD CONSTRAINT skill_consumption_debit CHECK(
  after_count>=0 AND after_count<before_count AND
  ((consumption_kind='reagent' AND before_count-after_count=1 AND hit_count=1) OR
   (consumption_kind='ammunition' AND before_count-after_count<=hit_count))),
 ADD CONSTRAINT skill_consumption_cast_item UNIQUE(cast_id,item_id);
COMMENT ON COLUMN app_world.skill_item_consumptions.cast_id IS
 'One sequence-reserved identity shared by all ordered stack receipts in an atomic cast. Existing receipts each retain a separate cast. No retry after unknown outcome.';
COMMENT ON COLUMN app_world.skill_item_consumptions.hit_count IS
 'For ammunition, 1..16 flagged non-expanded targets; pinned weapons use one unit plus additional hits. Reagent receipts retain 1 independently of target count.';
COMMENT ON COLUMN app_world.skill_item_consumptions.consumption_kind IS
 'Pinned skill reagent or ammunition kind of a powered compatible weapon. Ammo selects the first sufficient bag using original BYTE arithmetic, then consumes its ordered stacks. Expanded multi-attacks and absent cash templates remain unsupported.';
COMMENT ON TABLE app_world.skill_item_consumptions IS
 'Append-only per-stack receipts grouped by cast_id. All selected stack debits, core, learned timers and recovery checkpoint commit together before item/success ACKs. Contract 3 validates full owned rows; contract 2 validates the complete graph without stale child-table writes. Original uint64 IDs retain signed bigint bits. Character deletion cascades receipts.';
