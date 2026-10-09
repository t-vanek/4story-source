-- Preserve exact legacy text bytes alongside decoded, typed snapshots.
-- Needed because SQL Server code page metadata alone cannot diagnose mojibake.
CREATE TABLE reconstruction.source_text_bytes (
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    source_database text NOT NULL, source_schema text NOT NULL, source_table text NOT NULL,
    source_row_number bigint NOT NULL CHECK (source_row_number > 0),
    column_name text NOT NULL, source_bytes bytea,
    PRIMARY KEY (run_id, source_database, source_schema, source_table, source_row_number, column_name)
);
ALTER TABLE reconstruction.checkpoints ADD COLUMN source_bytes_sha256 text;
ALTER TABLE reconstruction.checkpoints ADD COLUMN target_bytes_sha256 text;
ALTER TABLE reconstruction.checkpoints ADD CONSTRAINT text_bytes_match CHECK (
    (source_bytes_sha256 IS NULL AND target_bytes_sha256 IS NULL) OR
    (source_bytes_sha256 IS NOT NULL AND target_bytes_sha256 IS NOT NULL AND source_bytes_sha256 = target_bytes_sha256)
);
