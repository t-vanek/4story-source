-- Written only. Distinguish item-action acceptance from learned/loop casts
-- without changing the original client protocol or rewriting old receipts.
-- Historical NULL origins still identify the pre-ACT learned/loop format by
-- is_loop; new writers record their exact opcode explicitly. Never classify
-- an item action by request length or borrow a learned rank/cost/charge.
ALTER TABLE app_world.accepted_skill_casts
    ADD COLUMN request_opcode integer,
    ADD COLUMN actual_channel smallint CHECK(actual_channel BETWEEN 0 AND 255),
    ADD COLUMN actual_map integer CHECK(actual_map BETWEEN 0 AND 65535);

ALTER TABLE app_world.accepted_skill_casts
    ADD CONSTRAINT accepted_skill_cast_origin CHECK(CASE WHEN request_opcode IS NULL
        THEN actual_channel IS NULL AND actual_map IS NULL
        ELSE (request_opcode=21172 AND NOT is_loop AND actual_channel IS NULL AND actual_map IS NULL)
        OR (request_opcode=21362 AND is_loop AND actual_channel IS NULL AND actual_map IS NULL)
        OR (request_opcode=22006 AND NOT is_loop AND actual_channel IS NOT NULL AND actual_map IS NOT NULL
            AND skill_rank=1 AND charge_action_id IS NULL
            AND octet_length(request)=36 AND octet_length(acknowledgement)=67)
        END);

COMMENT ON COLUMN app_world.accepted_skill_casts.request_opcode IS
 'Original CS_SKILLUSE_REQ21172, CS_LOOPSKILL_REQ21362 or CS_ACTITEMSUSE_REQ22006. Historical NULL preserves the old learned/loop receipt format; no backfill or fake ACT history. ACT skill_rank1 is its wire acceptance rank, not the later actual trigger-derived effect rank.';
COMMENT ON COLUMN app_world.accepted_skill_casts.actual_channel IS
 'ACT-only authoritative local channel at acceptance. The source parses but ignores the ACT request routing bytes; finish correlation uses this actual context instead. No new client field.';
COMMENT ON COLUMN app_world.accepted_skill_casts.actual_map IS
 'ACT-only unsigned WORD map at acceptance. Current-context finish matching cannot interpret client-supplied ignored route bytes as authority. Existing owner/session/checkpoint and one-use finish fences still apply.';
