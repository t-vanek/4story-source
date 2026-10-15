-- Written only. Source CSHandler.cpp20607-20614 returns from the whole
-- FINISHSKILL handler when3604 has no actual learned3603 object. Distinguish
-- that terminal no-effect outcome from completed Defend/equipment work.
-- Existing receipts retain NULL: do not invent historical dispositions.
ALTER TABLE app_world.skill_finish_operations
    ADD COLUMN disposition text;

ALTER TABLE app_world.skill_finish_operations
    ADD CONSTRAINT skill_finish_disposition CHECK (
        disposition IS NULL OR disposition='completed' OR
        (disposition='source_missing_3603'
            AND get_byte(request,21)=20 AND get_byte(request,22)=14
            AND get_byte(request,33)>0
            AND before_hp>0 AND before_hp=after_hp AND before_mp=after_mp
            AND responses='[]'::jsonb AND response_audiences IS NOT NULL
            AND response_audiences='[]'::jsonb
            AND before_effects IS NOT NULL AND after_effects IS NOT NULL
            AND before_effects=after_effects));

COMMENT ON COLUMN app_world.skill_finish_operations.disposition IS
 'Server-only terminal outcome: completed or source_missing_3603. NULL preserves historical receipts without backfill. Missing3603 requires a nonempty source3604 request, unchanged bars/effects and no output; it advances the existing one-use accepted-cast/finish head atomically with the checkpoint, without Defend or terminal CheckEquipSkill. This is not a success ACK, refund, new client field or permission to retry after uncertain COMMIT.';
