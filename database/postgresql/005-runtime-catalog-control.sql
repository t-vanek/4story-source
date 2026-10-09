-- Derived runtime catalog selection. Original snapshot columns/values are unchanged.
CREATE SCHEMA runtime_control;
CREATE SCHEMA game_compat;

CREATE TABLE runtime_control.active_catalog (
    singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE runtime_control.catalog_activations (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    previous_run_id bigint REFERENCES reconstruction.import_runs(id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL,
    activated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
COMMENT ON SCHEMA game_compat IS
    'Derived read-only C++ chart contracts; the pinned .bak files and legacy snapshots remain authoritative';
COMMENT ON TABLE runtime_control.active_catalog IS
    'Explicit verified reference release; never implicitly select the latest import. Change using activate_catalog.py';
REVOKE ALL ON SCHEMA runtime_control, game_compat FROM PUBLIC;
REVOKE ALL ON ALL TABLES IN SCHEMA runtime_control FROM PUBLIC;
