-- Fencing for the one active Login owner of this application database.
-- Operational metadata only; no historical records or credentials are changed.
CREATE TABLE app_global.login_runtime_owner (
    singleton boolean PRIMARY KEY DEFAULT true CHECK (singleton),
    owner_token text NOT NULL CHECK (owner_token ~ '^[0-9a-f]{64}$'),
    backend_pid integer NOT NULL,
    acquired_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP
);
REVOKE ALL ON app_global.login_runtime_owner FROM PUBLIC;
COMMENT ON TABLE app_global.login_runtime_owner IS
    'The owner holds a dedicated PostgreSQL advisory lock. Every Login write transaction locks this row FOR SHARE and verifies its process token; takeover waits for preceding transactions and fences the previous process.';
