-- Runtime copies of the three original TGLOBAL Patch metadata tables.
-- Historical values and original varchar bytes remain in legacy_global and
-- reconstruction.source_text_bytes. No files or versions are fabricated here.
CREATE SCHEMA app_patch;
CREATE TABLE app_patch."TVERSION" (
    "dwVersion" integer PRIMARY KEY,
    "szPath" text NOT NULL, "szName" text NOT NULL,
    path_bytes bytea NOT NULL, name_bytes bytea NOT NULL,
    "dwSize" integer NOT NULL DEFAULT 0, "dwBetaVer" integer,
    CHECK (octet_length(path_bytes)<=260 AND octet_length(name_bytes)<=260),
    UNIQUE ("szPath","szName")
);
CREATE TABLE app_patch."TPREVERSION" (
    "dwBetaVer" integer PRIMARY KEY,
    "szPath" text NOT NULL, "szName" text NOT NULL,
    path_bytes bytea NOT NULL, name_bytes bytea NOT NULL,
    "dwSize" integer NOT NULL,
    CHECK (octet_length(path_bytes)<=260 AND octet_length(name_bytes)<=260)
);
CREATE TABLE app_patch."TUSER_INTERFACE" (
    "bOption" smallint PRIMARY KEY CHECK ("bOption" BETWEEN 0 AND 255),
    "szName" text NOT NULL, name_bytes bytea NOT NULL CHECK (octet_length(name_bytes)<=260),
    "dwSize" double precision NOT NULL
);
CREATE TABLE app_patch.runtime_contract (
    singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
    contract integer NOT NULL CHECK (contract=1),
    source_run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    source_manifest text NOT NULL CHECK (source_manifest ~ '^[0-9a-f]{64}$'),
    min_beta integer NOT NULL,
    min_beta_definition_sha256 text NOT NULL CHECK (min_beta_definition_sha256 ~ '^[0-9a-f]{64}$'),
    imported_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
COMMENT ON TABLE app_patch.runtime_contract IS
    'Initialized only by explicit archival import. TMinBetaVer backup OUT parameter is authoritative; original CSP return-value call disagrees. Missing TPreCompleteAdd is not replaced by guessed promotion.';
COMMENT ON CONSTRAINT "TVERSION_szPath_szName_key" ON app_patch."TVERSION" IS
    'Exact decoded keys; SQL Server collation equivalence for future metadata writes is still required. Current runtime role is read-only.';
REVOKE ALL ON SCHEMA app_patch FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA app_patch FROM PUBLIC;
