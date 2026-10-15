-- Phase A: written only. Offline import must publish both complete historical
-- tables before World can hydrate. No archival row is changed or invented.
CREATE TABLE runtime_control.world_rps_catalog (
    world_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    run_id bigint NOT NULL REFERENCES reconstruction.import_runs(id),
    manifest_sha256 text NOT NULL CHECK(manifest_sha256 ~ '^[0-9a-f]{64}$'),
    chart_sha256 text NOT NULL CHECK(chart_sha256 ~ '^[0-9a-f]{64}$'),
    records_sha256 text NOT NULL CHECK(records_sha256 ~ '^[0-9a-f]{64}$'),
    chart_rows bigint NOT NULL CHECK(chart_rows BETWEEN 0 AND 7279),
    record_rows bigint NOT NULL CHECK(record_rows >= 0),
    source_timezone text NOT NULL CHECK(length(source_timezone)>0),
    imported_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE VIEW world_compat.rps_release AS
    SELECT c.* FROM runtime_control.world_rps_catalog c
    JOIN reconstruction.import_runs r ON r.id=c.run_id AND r.manifest_sha256=c.manifest_sha256
    JOIN reconstruction.checkpoints g ON g.run_id=c.run_id AND g.source_database='TGAME_RAGEZONE'
        AND g.source_schema='dbo' AND g.source_table='TRPSGAMECHART' AND g.row_count=c.chart_rows
        AND g.source_sha256=c.chart_sha256 AND g.target_sha256=c.chart_sha256
    JOIN reconstruction.checkpoints h ON h.run_id=c.run_id AND h.source_database='TGAME_RAGEZONE'
        AND h.source_schema='dbo' AND h.source_table='TRPSGAMERECORDTABLE' AND h.row_count=c.record_rows
        AND h.source_sha256=c.records_sha256 AND h.target_sha256=c.records_sha256
    WHERE r.status='verified';
CREATE VIEW world_compat."TRPSGAMECHART" AS
    SELECT c.world_id,s.* FROM legacy_game."TRPSGAMECHART" s
    JOIN world_compat.rps_release c ON c.run_id=s."_import_run_id";

CREATE TABLE app_world.rps_records (
    record_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    game_type smallint NOT NULL CHECK(game_type BETWEEN 0 AND 255),
    win_count smallint NOT NULL CHECK(win_count BETWEEN 0 AND 255),
    win_time timestamp without time zone NOT NULL CHECK(win_time=date_trunc('minute',win_time)
        AND win_time BETWEEN timestamp '1900-01-01' AND timestamp '2079-06-06 23:59:00'),
    hydrate_second bigint NOT NULL CHECK(hydrate_second BETWEEN -3000000000 AND 4000000000),
    -- Source INT bit patterns, including historical deleted/missing characters.
    -- Boot uses only game key/time. No invented operational character mapping.
    character_id integer NOT NULL,
    source_run_id bigint REFERENCES reconstruction.import_runs(id),
    source_row bigint CHECK(source_row>0),
    CHECK((source_run_id IS NULL)=(source_row IS NULL)),
    UNIQUE(world_id,source_run_id,source_row)
);
CREATE INDEX rps_records_expiry ON app_world.rps_records(world_id,game_type,win_count,win_time);
-- Source __TIMETODB uses local CTime and maps0 to1900; SQL SMALLDATETIME rounds
-- integral second30 upward. __DBTOTIME maps every year<2000 back to0. Resolve
-- later local minutes explicitly; ambiguous/nonexistent or sub-minute-offset
-- source calendars are unavailable instead of choosing an unproven CRT fold.
CREATE FUNCTION app_world.rps_source_minute(observed bigint,zone text)
RETURNS TABLE(win_time timestamp without time zone,hydrate_second bigint)
LANGUAGE plpgsql STABLE AS $$
DECLARE wall_epoch bigint; matches integer; resolved bigint;
BEGIN
    IF observed NOT BETWEEN -3000000000 AND 4000000000 THEN
        RAISE EXCEPTION 'RPS timestamp domain unavailable';
    END IF;
    IF observed=0 THEN
        win_time:=timestamp '1900-01-01'; hydrate_second:=0;
        RETURN NEXT; RETURN;
    END IF;
    win_time:=date_trunc('minute',(to_timestamp(observed) AT TIME ZONE zone)+interval '30 seconds');
    IF win_time NOT BETWEEN timestamp '1900-01-01' AND timestamp '2079-06-06 23:59:00' THEN
        RAISE EXCEPTION 'RPS SMALLDATETIME domain unavailable';
    END IF;
    IF extract(year FROM win_time)<2000 THEN
        hydrate_second:=0; RETURN NEXT; RETURN;
    END IF;
    wall_epoch:=extract(epoch FROM win_time)::bigint;
    SELECT count(*)::integer,min(wall_epoch+candidate) INTO matches,resolved
    FROM generate_series(-86400,86400,60) AS offsets(candidate)
    WHERE (to_timestamp(wall_epoch+candidate) AT TIME ZONE zone)=win_time;
    IF matches<>1 THEN
        RAISE EXCEPTION 'RPS calendar minute is ambiguous or unavailable';
    END IF;
    hydrate_second:=resolved; RETURN NEXT;
END;
$$;
CREATE TABLE app_world.rps_operations (
    operation_id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    owner_token text NOT NULL CHECK(owner_token ~ '^[0-9a-f]{64}$'),
    character_id integer NOT NULL,
    session_key integer NOT NULL CHECK(session_key<>0),
    server_id smallint NOT NULL CHECK(server_id BETWEEN 1 AND 255),
    game_type smallint NOT NULL CHECK(game_type BETWEEN 0 AND 255),
    win_count smallint NOT NULL CHECK(win_count BETWEEN 0 AND 255),
    request_second bigint NOT NULL,
    allowed boolean NOT NULL,
    operation_count integer NOT NULL CHECK(operation_count BETWEEN 0 AND 65536),
    committed_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
COMMENT ON TABLE app_world.rps_records IS
 'Native source TRPSGameRecord: duplicates retained; expiry deletes every same-key stored wall minute <= supplied rounded local minute. Live RAM retains unrounded seconds; restart uses explicit source timezone and pre2000 zero sentinel. Ambiguous calendar minutes require reconciliation.';
COMMENT ON TABLE app_world.rps_operations IS
 'Native commit-before-result boundary. No wire nonce or replay deduplication is implied. Unknown COMMIT stops World; inspect receipts/records before restart.';
REVOKE ALL ON runtime_control.world_rps_catalog,world_compat.rps_release,
    world_compat."TRPSGAMECHART",app_world.rps_records,app_world.rps_operations FROM PUBLIC;
REVOKE ALL ON FUNCTION app_world.rps_source_minute(bigint,text) FROM PUBLIC;
