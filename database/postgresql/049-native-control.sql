-- Native Control projections and operations. Original source tables, including
-- fields unused by Control, stay unchanged in legacy_global. No seeded accounts,
-- topology, cash templates, historical event times or patch files.
CREATE SCHEMA app_control;
REVOKE ALL ON SCHEMA app_control FROM PUBLIC;

CREATE TABLE app_control.runtime_contract (
    singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
    contract integer NOT NULL CHECK(contract=1),
    source_run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    source_manifest text NOT NULL CHECK(source_manifest ~ '^[0-9a-f]{64}$'),
    legacy_timezone text NOT NULL,
    text_encoding text NOT NULL CHECK(text_encoding='WIN1252'),
    imported_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE app_control.writer (
    singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
    generation bigint NOT NULL CHECK(generation>0),
    backend_pid integer NOT NULL,
    backend_start timestamptz NOT NULL
);
COMMENT ON TABLE app_control.writer IS
    'New operational single-Control writer fence, backed by a lifetime PostgreSQL advisory lock. Not a reconstructed legacy table.';

CREATE TABLE app_control.operators (
    name text COLLATE app_global.account_name_ci PRIMARY KEY,
    name_bytes bytea NOT NULL CHECK(octet_length(name_bytes)<=50),
    password_salt bytea NOT NULL CHECK(octet_length(password_salt)=16),
    password_hash bytea NOT NULL CHECK(octet_length(password_hash)=32),
    password_iterations integer NOT NULL CHECK(password_iterations BETWEEN 210000 AND 2000000),
    authority smallint NOT NULL CHECK(authority BETWEEN 0 AND 255)
);
CREATE UNIQUE INDEX control_operator_trimmed_name ON app_control.operators(rtrim(name));
COMMENT ON TABLE app_control.operators IS
    'TOPLogin replacement: original raw credential is transformed offline to PBKDF2-SHA256, with exact password case and trailing-space SQL padding semantics. Historical plaintext remains only in the restricted archive.';

CREATE TABLE app_control."TGROUP" (
    "bGroupID" smallint PRIMARY KEY CHECK("bGroupID" BETWEEN 0 AND 255), name_bytes bytea NOT NULL
);
CREATE TABLE app_control."TMACHINE" (
    "bMachineID" smallint PRIMARY KEY CHECK("bMachineID" BETWEEN 0 AND 255),
    name_bytes bytea NOT NULL, "bRouteID" smallint NOT NULL CHECK("bRouteID" BETWEEN 0 AND 255)
);
CREATE TABLE app_control."TNETWORK" (
    "bMachineID" smallint PRIMARY KEY CHECK("bMachineID" BETWEEN 0 AND 255), network_bytes bytea NOT NULL
);
CREATE TABLE app_control."TIPADDR" (
    "bMachineID" smallint NOT NULL CHECK("bMachineID" BETWEEN 0 AND 255),
    public_bytes bytea NOT NULL, private_bytes bytea NOT NULL,
    "bActive" smallint NOT NULL CHECK("bActive" BETWEEN 0 AND 255),
    PRIMARY KEY("bMachineID",public_bytes)
);
CREATE TABLE app_control."TSVRTYPE" (
    "bType" smallint PRIMARY KEY CHECK("bType" BETWEEN 0 AND 255), name_bytes bytea NOT NULL,
    "bControl" smallint NOT NULL CHECK("bControl" BETWEEN 0 AND 255)
);
CREATE TABLE app_control."TSERVER" (
    "bGroupID" smallint NOT NULL CHECK("bGroupID" BETWEEN 0 AND 255),
    "bServerID" smallint NOT NULL CHECK("bServerID" BETWEEN 0 AND 255),
    "bType" smallint NOT NULL CHECK("bType" BETWEEN 0 AND 255),
    "bMachineID" smallint NOT NULL CHECK("bMachineID" BETWEEN 0 AND 255),
    "wPort" smallint NOT NULL, name_bytes bytea NOT NULL,
    PRIMARY KEY("bGroupID","bType","bServerID")
);
COMMENT ON TABLE app_control."TSERVER" IS
    'Duplicate source identities must be rejected explicitly during activation; the archive retains all source rows. Signed wPort stores original WORD bits.';
CREATE TABLE app_control."TCASHSHOPITEMCHART" (
    "wID" smallint PRIMARY KEY, name_bytes bytea NOT NULL,
    "bCanSell" smallint NOT NULL CHECK("bCanSell" BETWEEN 0 AND 255)
);
COMMENT ON TABLE app_control."TCASHSHOPITEMCHART" IS
    'Control item-picker projection only. This does not replace the full cash-shop catalog or commerce implementation.';

CREATE TABLE app_control."TEVENTCHART" (
    "dwIndex" integer PRIMARY KEY,
    "bID" smallint NOT NULL CHECK("bID" BETWEEN 0 AND 255),
    title_bytes bytea NOT NULL CHECK(octet_length(title_bytes)<=256),
    "bGroupID" smallint NOT NULL CHECK("bGroupID" BETWEEN 0 AND 255),
    "bSvrType" smallint NOT NULL CHECK("bSvrType" BETWEEN 0 AND 255),
    "bSvrID" smallint NOT NULL CHECK("bSvrID" BETWEEN 0 AND 255),
    "dStartDate" timestamp NOT NULL, "dEndDate" timestamp NOT NULL,
    "wValue" smallint NOT NULL, "wMapID" smallint NOT NULL,
    "dwStartAlarm" integer NOT NULL, "dwEndAlarm" integer NOT NULL,
    "bPartTime" smallint NOT NULL CHECK("bPartTime" BETWEEN 0 AND 255),
    start_bytes bytea NOT NULL CHECK(octet_length(start_bytes)<=1024),
    mid_bytes bytea NOT NULL CHECK(octet_length(mid_bytes)<=1024),
    end_bytes bytea NOT NULL CHECK(octet_length(end_bytes)<=1024),
    value_bytes bytea NOT NULL CHECK(octet_length(value_bytes)<=1024)
);
COMMENT ON TABLE app_control."TEVENTCHART" IS
    'TEventUpdate typed transaction replacement. Dates preserve original local SMALLDATETIME values; runtime_contract names the operator-selected source timezone. No timezone is guessed from a backup.';

ALTER TABLE app_global."TUSERPROTECTED"
    ADD COLUMN "bWorld" smallint CHECK("bWorld" BETWEEN 0 AND 255),
    ADD COLUMN "dwCharID" integer,
    ADD COLUMN "szCharName" varchar(20),
    ADD COLUMN "regDate" timestamptz;
COMMENT ON COLUMN app_global."TUSERPROTECTED"."bWorld" IS
    'TUserProtectedAdd in the backup hardcodes world 1 and queries TGAME_GSP. Native replacement uses app_world for world 1; no arbitrary cross-world name lookup.';

CREATE UNIQUE INDEX patch_path_name_ci ON app_patch."TVERSION"
    (rtrim("szPath") COLLATE app_global.account_name_ci,rtrim("szName") COLLATE app_global.account_name_ci);
COMMENT ON INDEX app_patch.patch_path_name_ci IS
    'Explicit modern ICU case-insensitive accent-sensitive policy with SQL trailing-space comparison; complete Latin1_General_CI_AS_KS equivalence remains Phase B acceptance.';

CREATE TABLE runtime_control.operator_audit (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    operator_bytes bytea NOT NULL, authority smallint NOT NULL CHECK(authority BETWEEN 0 AND 255),
    remote_ip text NOT NULL, action text NOT NULL, target_bytes bytea NOT NULL, detail_bytes bytea NOT NULL,
    action_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE INDEX operator_audit_time ON runtime_control.operator_audit(action_at);
COMMENT ON TABLE runtime_control.operator_audit IS 'New native diagnostic audit; not fabricated TMANAGERLOG history.';
REVOKE ALL ON ALL TABLES IN SCHEMA app_control FROM PUBLIC;
REVOKE ALL ON runtime_control.operator_audit FROM PUBLIC;
REVOKE ALL ON SEQUENCE runtime_control.operator_audit_id_seq FROM PUBLIC;
