-- Native source operator IDs and durable chat-ban authority. No source data is
-- seeded. Original bans are World memory only; durable recovery is an explicit
-- modernization. Old migrations and faithful archival rows remain immutable.
CREATE TABLE app_world."TOPERATORTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwOperatorID" integer NOT NULL,
    PRIMARY KEY("bWorldID","dwOperatorID"),
    FOREIGN KEY("bWorldID","dwOperatorID") REFERENCES app_world."TCHARTABLE"
);
CREATE TABLE reconstruction.operator_imports (
    world_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    character_mapping_sha256 text NOT NULL CHECK(character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    value_sha256 text NOT NULL CHECK(value_sha256 ~ '^[0-9a-f]{64}$'),
    native_sha256 text NOT NULL CHECK(native_sha256 ~ '^[0-9a-f]{64}$'),
    source_rows bigint NOT NULL CHECK(source_rows=0),
    operator_rows bigint NOT NULL CHECK(operator_rows=source_rows),
    imported_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE VIEW world_compat.operator_release AS
    SELECT o.world_id,o.run_id,o.source_rows,o.operator_rows,o.character_mapping_sha256,r.status
    FROM reconstruction.operator_imports o
    JOIN reconstruction.import_runs r ON r.id=o.run_id AND r.status='verified'
    JOIN reconstruction.checkpoints p ON p.run_id=o.run_id AND p.source_database='TGAME_RAGEZONE'
        AND p.source_schema='dbo' AND p.source_table='TOPERATORTABLE' AND p.row_count=o.source_rows
        AND p.source_sha256=o.value_sha256 AND p.target_sha256=o.value_sha256
    JOIN LATERAL (SELECT count(*) AS row_count,
        encode(sha256(convert_to(COALESCE(string_agg(("dwOperatorID"::bigint & 4294967295)::text,','
            ORDER BY ("dwOperatorID"::bigint & 4294967295)),''),'UTF8')),'hex') AS native_hash
        FROM app_world."TOPERATORTABLE" WHERE "bWorldID"=o.world_id) n
        ON n.row_count=o.operator_rows AND n.native_hash=o.native_sha256;
CREATE TABLE app_world.chat_bans (
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    name_bytes bytea NOT NULL CHECK(octet_length(name_bytes) BETWEEN 1 AND 50),
    ends_at bigint NOT NULL CHECK(ends_at>=0),
    PRIMARY KEY(world_id,name_bytes)
);
CREATE TABLE app_world.chat_ban_mutations (
    mutation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
    target_id integer NOT NULL, name_bytes bytea NOT NULL,
    issuer_id integer NOT NULL, session_key integer NOT NULL,
    source_server smallint NOT NULL CHECK(source_server BETWEEN 0 AND 255),
    source_map_token text CHECK(source_map_token ~ '^[0-9a-f]{64}$'),
    source_connection_id bigint CHECK(source_connection_id>0),
    source_authority_epoch bigint CHECK(source_authority_epoch>=0),
    minutes integer NOT NULL CHECK(minutes BETWEEN 0 AND 65535),
    replace_deadline boolean NOT NULL, ends_at bigint NOT NULL CHECK(ends_at>=0),
    committed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    CHECK((source_server=0 AND source_map_token IS NULL AND source_connection_id IS NULL AND source_authority_epoch IS NULL)
       OR (source_server>0 AND source_map_token IS NOT NULL AND source_connection_id IS NOT NULL AND source_authority_epoch IS NOT NULL))
);
COMMENT ON VIEW world_compat.operator_release IS
 'Full verified TOPERATORTABLE projection with exact explicit character mapping and native ID checksum. A verified empty archive is legitimate; implicit empty defaults and GM-name heuristics are not authorization.';
COMMENT ON TABLE app_world.chat_bans IS
 'Original exact raw-name World ban map made durable. Missing/expired row means no active ban; no historical ban table is asserted. Ban deadlines are wall-clock seconds, outside original packet fields.';
REVOKE ALL ON app_world."TOPERATORTABLE",app_world.chat_bans,app_world.chat_ban_mutations,
    reconstruction.operator_imports,world_compat.operator_release FROM PUBLIC;
