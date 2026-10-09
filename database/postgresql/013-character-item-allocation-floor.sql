-- Confirmed TGAME_RAGEZONE.TDBITEMINDEXTABLE: bWorld=0, dlID=735812.
-- New mutable world-zero allocation must never reuse identifiers below that
-- historical high-water even though the backup's TITEMTABLE itself is empty.
ALTER TABLE app_world.worlds ADD CONSTRAINT world_zero_original_high_water
    CHECK (item_world<>0 OR item_high_water>=735812);
