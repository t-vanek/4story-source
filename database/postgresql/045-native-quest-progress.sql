-- Source TSaveQuest / TSaveQuestTerm semantics, world-scoped. Archival rows
-- are never rewritten. Import is an explicit offline operation, not a startup
-- side effect. The supplied backup has zero rows in both progress tables.
CREATE TABLE app_world."TQUESTTABLE" (
    "bWorldID" smallint NOT NULL,
    "dwCharID" integer NOT NULL,
    "dwQuestID" integer NOT NULL,
    "dwTick" integer NOT NULL,
    "bCompleteCount" smallint NOT NULL CHECK ("bCompleteCount" BETWEEN 0 AND 255),
    "bTriggerCount" smallint NOT NULL CHECK ("bTriggerCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("bWorldID", "dwCharID", "dwQuestID"),
    FOREIGN KEY ("bWorldID", "dwCharID") REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE TABLE app_world."TQUESTTERMTABLE" (
    "bWorldID" smallint NOT NULL,
    "dwCharID" integer NOT NULL,
    "dwQuestID" integer NOT NULL,
    "dwTermID" integer NOT NULL,
    "bTermType" smallint NOT NULL CHECK ("bTermType" BETWEEN 0 AND 255),
    "bCount" smallint NOT NULL CHECK ("bCount" BETWEEN 0 AND 255),
    PRIMARY KEY ("bWorldID", "dwCharID", "dwQuestID", "dwTermID"),
    FOREIGN KEY ("bWorldID", "dwCharID", "dwQuestID")
        REFERENCES app_world."TQUESTTABLE" ON DELETE CASCADE
);
COMMENT ON TABLE app_world."TQUESTTERMTABLE" IS
    'Source primary key excludes term type. TSaveQuestTerm updates count without changing type. Full Map transfer graphs remain authoritative after handoff; their older normalized progress is not reloaded.';
CREATE TABLE app_world.quest_operations (
    operation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id smallint NOT NULL,
    char_id integer NOT NULL,
    previous_operation_id bigint NOT NULL CHECK (previous_operation_id >= 0),
    session_key bigint NOT NULL CHECK (session_key BETWEEN 1 AND 4294967295),
    server_id smallint NOT NULL,
    owner_token text NOT NULL CHECK (owner_token ~ '^[0-9a-f]{64}$'),
    connection_id bigint NOT NULL CHECK (connection_id > 0),
    authority_epoch bigint NOT NULL CHECK (authority_epoch >= 0),
    opcode integer NOT NULL CHECK (opcode BETWEEN 0 AND 65535),
    request bytea NOT NULL,
    before_state jsonb NOT NULL CHECK (jsonb_typeof(before_state) = 'array'),
    after_state jsonb NOT NULL CHECK (jsonb_typeof(after_state) = 'array'),
    responses jsonb NOT NULL CHECK (jsonb_typeof(responses) = 'array'),
    committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    CHECK (operation_id > previous_operation_id),
    UNIQUE (world_id, char_id, previous_operation_id),
    FOREIGN KEY (world_id, char_id) REFERENCES app_world."TCHARTABLE" ON DELETE CASCADE
);
CREATE FUNCTION app_world.map_quest_state(p_world smallint, p_char integer)
RETURNS jsonb LANGUAGE sql STABLE AS $$
    SELECT COALESCE(jsonb_agg(jsonb_build_array(
        q."dwQuestID"::bigint & 4294967295, q."dwTick"::bigint & 4294967295,
        q."bCompleteCount", q."bTriggerCount",
        COALESCE((SELECT jsonb_agg(jsonb_build_array(t."dwTermID"::bigint & 4294967295,
            t."bTermType", t."bCount") ORDER BY t."dwTermID"::bigint & 4294967295)
            FROM app_world."TQUESTTERMTABLE" t WHERE t."bWorldID"=q."bWorldID"
            AND t."dwCharID"=q."dwCharID" AND t."dwQuestID"=q."dwQuestID"), '[]'::jsonb))
        ORDER BY q."dwQuestID"::bigint & 4294967295), '[]'::jsonb)
    FROM app_world."TQUESTTABLE" q WHERE q."bWorldID"=p_world AND q."dwCharID"=p_char
$$;
ALTER TABLE app_world.map_checkpoints ADD COLUMN quest_state jsonb
    CHECK (quest_state IS NULL OR jsonb_typeof(quest_state)='array');
CREATE OR REPLACE FUNCTION app_world.map_checkpoint_matches(p app_world.map_checkpoints)
RETURNS boolean LANGUAGE sql STABLE AS $$
    SELECT COALESCE(p.core_state=app_world.map_core_state(p.world_id,p.char_id)
        AND p.recovery_contract IN (1,2,3,4)
        AND (p.recovery_contract NOT IN (3,4) OR p.skill_state=app_world.map_skill_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR
            COALESCE(p.maintain_state,'[]'::jsonb)=app_world.map_maintain_state(p.world_id,p.char_id))
        AND (p.recovery_contract=2 OR
            COALESCE(p.quest_state,'[]'::jsonb)=app_world.map_quest_state(p.world_id,p.char_id)), false)
$$;
CREATE TABLE reconstruction.quest_progress_imports (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    character_mapping_sha256 text NOT NULL CHECK (character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    quests bigint NOT NULL CHECK (quests >= 0),
    terms bigint NOT NULL CHECK (terms >= 0),
    imported_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY (run_id, world_id)
);
REVOKE ALL ON app_world."TQUESTTABLE", app_world."TQUESTTERMTABLE", app_world.quest_operations,
    reconstruction.quest_progress_imports FROM PUBLIC;
REVOKE ALL ON SEQUENCE app_world.quest_operations_operation_id_seq FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_quest_state(smallint, integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.map_checkpoint_matches(app_world.map_checkpoints) FROM PUBLIC;
