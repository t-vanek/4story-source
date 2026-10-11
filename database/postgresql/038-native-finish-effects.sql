-- Preserve historical finish bytes. New finishes record effect changes and
-- recipient classes separately from the unchanged client protocol.
ALTER TABLE app_world.skill_finish_operations
 ADD COLUMN before_effects jsonb,
 ADD COLUMN after_effects jsonb,
 ADD COLUMN response_audiences jsonb;
ALTER TABLE app_world.skill_finish_operations ADD CONSTRAINT finish_effect_contract CHECK(
 (before_effects IS NULL AND after_effects IS NULL AND response_audiences IS NULL)
 OR (before_effects IS NOT NULL AND after_effects IS NOT NULL AND response_audiences IS NOT NULL
     AND jsonb_typeof(before_effects)='array' AND jsonb_array_length(before_effects)<=255
     AND jsonb_typeof(after_effects)='array' AND jsonb_array_length(after_effects)<=255
     AND jsonb_typeof(response_audiences)='array'
     AND jsonb_array_length(response_audiences)=jsonb_array_length(responses)
     AND response_audiences <@ '[0,1,2]'::jsonb));
COMMENT ON COLUMN app_world.skill_finish_operations.response_audiences IS
 'Server-only recipient classes parallel to responses: 0=own PC, 1=neighbor cells, 2=nearby radius. Historical NULL keeps its original receipt unchanged.';
