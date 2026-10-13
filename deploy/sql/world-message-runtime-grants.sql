-- Written only. Existing001-068 + --server-messages publication are prerequisites.
-- Apply with the other World grants only during isolated later acceptance.
BEGIN;
GRANT USAGE ON SCHEMA game_compat,character_compat,actor_compat,reconstruction TO :"world_role";
GRANT SELECT ON game_compat.server_message_release,game_compat.server_messages,
    game_compat.catalog_release,character_compat.catalog_release,actor_compat.catalog_release,
    reconstruction.checkpoints TO :"world_role";
COMMIT;
