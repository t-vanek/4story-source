-- Operational evidence for native item mutations. Original m_wEventValue is a
-- live Map projection, not historical content. Preserve old operation receipts
-- with NULL: they were not sampled and must not be relabelled with zero values.
ALTER TABLE app_world.quest_operations ADD COLUMN world_event_context jsonb
    CHECK (CASE
        WHEN world_event_context IS NULL THEN true
        WHEN jsonb_typeof(world_event_context) <> 'array' THEN false
        WHEN jsonb_array_length(world_event_context) <> 3 THEN false
        WHEN jsonb_typeof(world_event_context->0) <> 'number'
          OR jsonb_typeof(world_event_context->1) <> 'number'
          OR jsonb_typeof(world_event_context->2) <> 'array' THEN false
        ELSE (world_event_context->>0)::numeric > 0
         AND (world_event_context->>1)::numeric >= 0
         AND jsonb_array_length(world_event_context->2) = 16
        END);
COMMENT ON COLUMN app_world.quest_operations.world_event_context IS
    '[World connection generation, received event revision, 16 WORD values]. Server-only observation at operation admission; later event changes do not rewrite committed receipts. NULL means no observation. Not a claim that Control event delivery/recovery is complete.';
