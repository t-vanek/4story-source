-- New operational ownership and source-shaped social persistence. Historical
-- archives and migrations001-065 are immutable. No player data is invented.
CREATE TABLE app_world.world_runtime_owner (
    world_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
    backend_pid integer NOT NULL,
    acquired_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE app_world."TFRIENDGROUPTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "bGroup" smallint NOT NULL CHECK("bGroup" BETWEEN 0 AND 255),
    "szName" text NOT NULL,
    name_bytes bytea NOT NULL CHECK(octet_length(name_bytes)<=20),
    PRIMARY KEY("bWorldID","dwCharID","bGroup"),
    FOREIGN KEY("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE"
);
CREATE TABLE app_world."TFRIENDTABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "dwFriendID" integer NOT NULL,
    "bGroup" smallint NOT NULL CHECK("bGroup" BETWEEN 0 AND 255),
    PRIMARY KEY("bWorldID","dwCharID","dwFriendID"),
    FOREIGN KEY("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE",
    FOREIGN KEY("bWorldID","dwFriendID") REFERENCES app_world."TCHARTABLE"
);
CREATE INDEX native_friend_reverse ON app_world."TFRIENDTABLE"("bWorldID","dwFriendID","dwCharID");
-- A source pairing is DIRECTED. Registration writes one row; inbound mates
-- have no independent row. dwTime is a DWORD bit pattern, not a PG timestamp.
CREATE TABLE app_world."TSOULMATETABLE" (
    "bWorldID" smallint NOT NULL REFERENCES app_world.worlds(group_id),
    "dwCharID" integer NOT NULL,
    "dwTarget" integer NOT NULL,
    "dwTime" integer NOT NULL DEFAULT 0,
    PRIMARY KEY("bWorldID","dwCharID"),
    FOREIGN KEY("bWorldID","dwCharID") REFERENCES app_world."TCHARTABLE",
    FOREIGN KEY("bWorldID","dwTarget") REFERENCES app_world."TCHARTABLE"
);
CREATE INDEX native_soulmate_reverse ON app_world."TSOULMATETABLE"("bWorldID","dwTarget","dwCharID");
CREATE TABLE app_world.social_mutations (
    mutation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
    char_id integer NOT NULL, session_key integer NOT NULL,
    source_server smallint NOT NULL CHECK(source_server BETWEEN 0 AND 255),
    kind text NOT NULL CHECK(kind IN ('friend_insert','friend_mutual','friend_erase','group_make',
        'group_delete','group_rename','group_change','soulmate_reg','soulmate_del','soulmate_end')),
    target_id integer, group_id smallint CHECK(group_id BETWEEN 0 AND 255),
    committed_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE reconstruction.social_imports (
    world_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    character_mapping_sha256 text NOT NULL CHECK(character_mapping_sha256 ~ '^[0-9a-f]{64}$'),
    source_varchar_encoding text NOT NULL, wire_encoding text NOT NULL,
    group_rows bigint NOT NULL CHECK(group_rows>=0), friend_rows bigint NOT NULL CHECK(friend_rows>=0),
    soulmate_rows bigint NOT NULL CHECK(soulmate_rows>=0),
    imported_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
COMMENT ON TABLE app_world."TFRIENDTABLE" IS
 'Authoritative directed social graph, separate from Map checkpoint/character blobs. Group references intentionally have no FK: source permits orphan group values and deleting a group does not reset edges.';
COMMENT ON TABLE app_world.world_runtime_owner IS
 'Dedicated 4WLD/world advisory lock and token row. Native World transactions lock/validate this row; this migration does not fence the still unported World repositories.';
REVOKE ALL ON app_world.world_runtime_owner,app_world."TFRIENDGROUPTABLE",app_world."TFRIENDTABLE",
    app_world."TSOULMATETABLE",app_world.social_mutations,reconstruction.social_imports FROM PUBLIC;
