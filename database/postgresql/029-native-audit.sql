-- Native append-only LP_LOG storage. Source: LogPacket.h and
-- CUdpSocket::LogDBSave/SQLQUERY_DGINSERT; the two supplied backups do not
-- contain the external ITEMLOGTLyyyymmdd audit tables. No history is invented.
-- CHAR fields retain their bytes without guessing a client code page.
CREATE SCHEMA app_audit;
REVOKE ALL ON SCHEMA app_audit FROM PUBLIC;
CREATE TABLE app_audit.runtime_contract (
    singleton boolean PRIMARY KEY DEFAULT true CHECK(singleton),
    version integer NOT NULL CHECK(version=1)
);
INSERT INTO app_audit.runtime_contract(singleton,version) VALUES(true,1);
CREATE TABLE app_audit."TLOG_AUDIT" (
    lt_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    lt_logdate timestamp without time zone NOT NULL,
    lt_serverid bigint NOT NULL CHECK(lt_serverid BETWEEN 0 AND 4294967295),
    lt_clientip bytea NOT NULL CHECK(octet_length(lt_clientip)<=16),
    lt_action bigint NOT NULL CHECK(lt_action BETWEEN 0 AND 4294967295),
    lt_mapid integer NOT NULL CHECK(lt_mapid BETWEEN 0 AND 65535),
    lt_x integer NOT NULL,
    lt_y integer NOT NULL,
    lt_z integer NOT NULL,
    lt_dwkey1 bigint NOT NULL,
    lt_dwkey2 bigint NOT NULL,
    lt_dwkey3 bigint NOT NULL,
    lt_dwkey4 bigint NOT NULL,
    lt_dwkey5 bigint NOT NULL,
    lt_dwkey6 bigint NOT NULL,
    lt_dwkey7 bigint NOT NULL,
    lt_dwkey8 bigint NOT NULL,
    lt_dwkey9 bigint NOT NULL,
    lt_dwkey10 bigint NOT NULL,
    lt_dwkey11 bigint NOT NULL,
    lt_key1 bytea NOT NULL CHECK(octet_length(lt_key1)<=50),
    lt_key2 bytea NOT NULL CHECK(octet_length(lt_key2)<=50),
    lt_key3 bytea NOT NULL CHECK(octet_length(lt_key3)<=50),
    lt_key4 bytea NOT NULL CHECK(octet_length(lt_key4)<=50),
    lt_key5 bytea NOT NULL CHECK(octet_length(lt_key5)<=50),
    lt_key6 bytea NOT NULL CHECK(octet_length(lt_key6)<=50),
    lt_key7 bytea NOT NULL CHECK(octet_length(lt_key7)<=50),
    lt_fmt bigint NOT NULL CHECK(lt_fmt BETWEEN 0 AND 4294967295),
    lt_log bytea CHECK(octet_length(lt_log)<=512),
    received_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE INDEX audit_event_date ON app_audit."TLOG_AUDIT"(lt_logdate,lt_id);
CREATE INDEX audit_action ON app_audit."TLOG_AUDIT"(lt_action,lt_id DESC);
CREATE INDEX audit_user ON app_audit."TLOG_AUDIT"(lt_dwkey1,lt_id DESC);
COMMENT ON TABLE app_audit."TLOG_AUDIT" IS
 'Native LP_LOG append-only contract 1. Original integer widths and raw CHAR bytes retained. Synthetic identity and received_at are modern metadata. No historical audit import or automatic retention deletion. Unknown COMMIT outcomes must not be replayed.';
COMMENT ON COLUMN app_audit."TLOG_AUDIT".lt_logdate IS
 'Original LogDBSave formatted DBTIMESTAMP to seconds without timezone; received_at is separate receipt time.';
COMMENT ON COLUMN app_audit."TLOG_AUDIT".lt_log IS
 'Exactly the transmitted nonempty payload; NULL for empty as in the existing modern sink. Original legacy LogDBSave always wrote 512 bytes.';
