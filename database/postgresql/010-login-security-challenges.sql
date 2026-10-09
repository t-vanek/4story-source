-- Modern application policy for the source client's email-code / LOGIN-retry flow.
-- This is operational state, not a reconstruction of historical TSECURECODE rows.
-- No account, credential, hardware identity or trusted-IP record is imported.
CREATE TABLE app_global.login_security_challenge (
    token text PRIMARY KEY CHECK (token ~ '^[0-9a-f]{64}$'),
    owner_token text NOT NULL CHECK (owner_token ~ '^[0-9a-f]{64}$'),
    user_id integer NOT NULL REFERENCES app_global."TACCOUNT_PW"("dwUserID"),
    client_ip text NOT NULL CHECK (length(client_ip) BETWEEN 1 AND 50),
    client_version integer NOT NULL CHECK (client_version = 10520), -- source TVERSION 0x2918
    code_digest text NOT NULL CHECK (code_digest ~ '^[0-9a-f]{64}$'),
    attempts integer NOT NULL DEFAULT 0 CHECK (attempts BETWEEN 0 AND 5),
    issued_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at timestamptz NOT NULL DEFAULT (CURRENT_TIMESTAMP + interval '5 minutes'),
    verified_at timestamptz,
    CHECK (expires_at > issued_at AND expires_at <= issued_at + interval '5 minutes'),
    CHECK (verified_at IS NULL OR verified_at BETWEEN issued_at AND expires_at)
);
CREATE INDEX login_security_challenge_user ON app_global.login_security_challenge(user_id);
REVOKE ALL ON app_global.login_security_challenge FROM PUBLIC;
COMMENT ON TABLE app_global.login_security_challenge IS
    'Modern policy: per-connection 256-bit token, six-character code hash, five attempts, five-minute expiry, single thirty-second verified LOGIN retry. Source non-direct 0x2918 client retries with form password, normalized to its initial SHA1 wire credential only with this verified grant. No persistent IP/MAC trust.';

-- Issuance budget survives disconnect/cancellation and process failover.
CREATE TABLE app_global.login_security_rate (
    user_id integer PRIMARY KEY REFERENCES app_global."TACCOUNT_PW"("dwUserID"),
    window_start timestamptz NOT NULL,
    issued_count integer NOT NULL CHECK (issued_count BETWEEN 1 AND 3)
);
REVOKE ALL ON app_global.login_security_rate FROM PUBLIC;
COMMENT ON TABLE app_global.login_security_rate IS
    'Modern per-account budget: at most three email challenges per five-minute issuance window, including cancelled challenges. Serialized by the authentication account row lock.';
