-- Migration 001: import control plane (PROPOSED).
CREATE SCHEMA reconstruction;
CREATE TABLE reconstruction.import_runs (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    manifest_sha256 text NOT NULL UNIQUE CHECK (manifest_sha256 ~ '^[0-9a-f]{64}$'),
    source_manifest jsonb NOT NULL,
    status text NOT NULL CHECK (status IN ('running','verified','failed')),
    started_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    verified_at timestamptz
);
CREATE TABLE reconstruction.checkpoints (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    source_database text NOT NULL, source_schema text NOT NULL, source_table text NOT NULL,
    row_count bigint NOT NULL CHECK (row_count >= 0),
    source_sha256 text NOT NULL, target_sha256 text NOT NULL,
    completed_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (run_id, source_database, source_schema, source_table),
    CHECK (source_sha256 = target_sha256)
);
CREATE TABLE reconstruction.failures (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    source_table text NOT NULL, error_code text NOT NULL,
    source_row_number bigint, -- no row payload, login, IP or credentials
    occurred_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
