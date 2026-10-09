-- Distinguish row-backed fresh inventory from authoritative full-graph state.
-- Historical schemas, backups and applied migrations 001-022 stay unchanged.
ALTER TABLE app_world.skill_item_consumptions
 ADD COLUMN state_contract smallint NOT NULL DEFAULT 3 CHECK(state_contract IN (2,3)),
 ADD COLUMN before_graph_hash text,
 ADD COLUMN after_graph_hash text,
 ADD CONSTRAINT skill_consumption_graph_receipt CHECK(
  (state_contract=3 AND before_graph_hash IS NULL AND after_graph_hash IS NULL) OR
  (state_contract=2 AND before_graph_hash IS NOT NULL AND after_graph_hash IS NOT NULL
   AND before_graph_hash ~ '^[0-9a-f]{64}$' AND after_graph_hash ~ '^[0-9a-f]{64}$'));
COMMENT ON TABLE app_world.skill_item_consumptions IS
 'Append-only native single-reagent receipts committed with core and timers before client success. Contract 3 hashes complete TITEMTABLE rows; contract 2 hashes original encoded server item records and records before/after full checkpoint graph hashes. Graph consumption changes only the authoritative transfer checkpoint, never stale child tables. Original uint64 item IDs retain their signed bigint bit pattern. No retry after unknown outcome. Character deletion cascades receipts.';
