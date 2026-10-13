-- Additive Phase B/C grants after001-067. Do not execute in Phase A.
-- Requires existing Map and World social/runtime grants; no historical writes.
BEGIN;
GRANT SELECT,INSERT,UPDATE ON app_world.soulmate_offers TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.soulmate_offers_offer_id_seq TO :"map_role";
GRANT INSERT,UPDATE ON app_world."TSOULMATETABLE" TO :"map_role";
GRANT INSERT ON app_world.social_mutations TO :"map_role";
GRANT USAGE ON SEQUENCE app_world.social_mutations_mutation_id_seq TO :"map_role";
GRANT SELECT,UPDATE(owner_token) ON app_world.world_runtime_owner TO :"map_role";
GRANT SELECT,UPDATE ON app_world.soulmate_offers TO :"world_role";
COMMIT;
