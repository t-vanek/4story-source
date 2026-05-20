-- Migration: legacy date-partitioned ITEMLOGTLyyyymmdd → modern
-- single-table TLOG_AUDIT. Idempotent — re-running it on already-
-- migrated tables is a no-op (UNION-ALL into TLOG_AUDIT is
-- destination-INSERT only, the source tables are left intact).
--
-- Why this migration is required: TLogSvrAsio's schema_validator
-- hard-fails on boot when TLOG_AUDIT doesn't exist. Operators
-- coming from a legacy deploy must apply tlog-audit.sql first
-- (creates the empty TLOG_AUDIT) and then this script to copy
-- their historical data.
--
-- Usage:
--   sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i tlog-audit.sql
--   sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE \
--          -i migrate-itemlogtl-to-tlog-audit.sql
--
-- Notes:
--   - Source tables stay untouched (no DROP). Operators decide
--     when to retire them after verifying counts.
--   - This script discovers ITEMLOGTL* tables dynamically via
--     INFORMATION_SCHEMA and emits one INSERT per day partition,
--     so no manual table list is required.
--   - Column mapping handles two known legacy schemas:
--     * 28-column (modern legacy): full 11 DWORD keys + 7 string
--       keys + format + blob.
--     * 17-column (older legacy): 4 DWORD keys + 4 string keys.
--       The missing columns get defaulted to 0 / '' in the
--       destination.
--   - Date math: legacy split LT_LOGDATE into separate year /
--     month / day BIGINT cols on some deploys; this script
--     re-assembles them into DATETIME if the source has them
--     split, else uses LT_LOGDATE as-is.

USE TGLOBAL_RAGEZONE;
GO

SET NOCOUNT ON;

-- Safety: ensure destination exists. Operators who skipped
-- tlog-audit.sql get a clear error here instead of a column
-- mismatch deep inside the dynamic INSERT below.
IF OBJECT_ID('TLOG_AUDIT', 'U') IS NULL
BEGIN
    RAISERROR('TLOG_AUDIT does not exist. Apply tlog-audit.sql before running this migration.', 16, 1);
    RETURN;
END

DECLARE @sql NVARCHAR(MAX);
DECLARE @tbl NVARCHAR(128);
DECLARE @rows_total BIGINT = 0;
DECLARE @rows_one   BIGINT;
DECLARE @t_count    INT = 0;

DECLARE cur CURSOR LOCAL FAST_FORWARD FOR
    SELECT TABLE_NAME
    FROM INFORMATION_SCHEMA.TABLES
    WHERE TABLE_NAME LIKE 'ITEMLOGTL[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]'
      AND TABLE_TYPE = 'BASE TABLE'
    ORDER BY TABLE_NAME;

OPEN cur;
FETCH NEXT FROM cur INTO @tbl;
WHILE @@FETCH_STATUS = 0
BEGIN
    SET @t_count = @t_count + 1;

    -- Detect the legacy column layout by checking for one of the
    -- "new-schema-only" columns. Older legacy tables don't have
    -- LT_DWKEY5..11 / LT_KEY5..7 / LT_FMT.
    DECLARE @has_new_cols BIT;
    SELECT @has_new_cols = CASE WHEN COUNT(*) >= 8 THEN 1 ELSE 0 END
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_NAME = @tbl
      AND COLUMN_NAME IN ('LT_DWKEY5','LT_DWKEY6','LT_DWKEY7',
                          'LT_DWKEY8','LT_DWKEY9','LT_DWKEY10',
                          'LT_DWKEY11','LT_FMT');

    -- Detect whether the source has LT_LOGDATE as DATETIME or as
    -- separate year/month/day columns.
    DECLARE @has_logdate BIT;
    SELECT @has_logdate = CASE WHEN COUNT(*) = 1 THEN 1 ELSE 0 END
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_NAME = @tbl AND COLUMN_NAME = 'LT_LOGDATE';

    DECLARE @date_expr NVARCHAR(200);
    IF @has_logdate = 1
        SET @date_expr = N'LT_LOGDATE';
    ELSE
        -- Reassemble from yyyy/mm/dd + hh/mm/ss columns. The exact
        -- name varies across deploys — these are the common ones.
        SET @date_expr = N'DATEFROMPARTS(LT_LOGYEAR, LT_LOGMONTH, LT_LOGDAY)';

    IF @has_new_cols = 1
    BEGIN
        SET @sql = N'
            INSERT INTO TLOG_AUDIT (
                LT_LOGDATE, LT_SERVERID, LT_CLIENTIP, LT_ACTION,
                LT_MAPID, LT_X, LT_Y, LT_Z,
                LT_DWKEY1, LT_DWKEY2, LT_DWKEY3, LT_DWKEY4,
                LT_DWKEY5, LT_DWKEY6, LT_DWKEY7, LT_DWKEY8,
                LT_DWKEY9, LT_DWKEY10, LT_DWKEY11,
                LT_KEY1, LT_KEY2, LT_KEY3, LT_KEY4,
                LT_KEY5, LT_KEY6, LT_KEY7,
                LT_FMT, LT_LOG)
            SELECT
                ' + @date_expr + N', LT_SERVERID, LT_CLIENTIP, LT_ACTION,
                LT_MAPID, LT_X, LT_Y, LT_Z,
                LT_DWKEY1, LT_DWKEY2, LT_DWKEY3, LT_DWKEY4,
                LT_DWKEY5, LT_DWKEY6, LT_DWKEY7, LT_DWKEY8,
                LT_DWKEY9, LT_DWKEY10, LT_DWKEY11,
                LT_KEY1, LT_KEY2, LT_KEY3, LT_KEY4,
                LT_KEY5, LT_KEY6, LT_KEY7,
                LT_FMT, LT_LOG
            FROM ' + QUOTENAME(@tbl) + N'
            WHERE NOT EXISTS (
                SELECT 1 FROM TLOG_AUDIT a
                WHERE a.LT_LOGDATE = ' + @date_expr + N'
                  AND a.LT_SERVERID = ' + QUOTENAME(@tbl) + N'.LT_SERVERID
                  AND a.LT_CLIENTIP = ' + QUOTENAME(@tbl) + N'.LT_CLIENTIP
                  AND a.LT_ACTION   = ' + QUOTENAME(@tbl) + N'.LT_ACTION
                  AND a.LT_DWKEY1   = ' + QUOTENAME(@tbl) + N'.LT_DWKEY1
            );';
    END
    ELSE
    BEGIN
        -- Older schema — defaults for the missing columns.
        SET @sql = N'
            INSERT INTO TLOG_AUDIT (
                LT_LOGDATE, LT_SERVERID, LT_CLIENTIP, LT_ACTION,
                LT_MAPID, LT_X, LT_Y, LT_Z,
                LT_DWKEY1, LT_DWKEY2, LT_DWKEY3, LT_DWKEY4,
                LT_KEY1, LT_KEY2, LT_KEY3, LT_KEY4,
                LT_LOG)
            SELECT
                ' + @date_expr + N', LT_SERVERID, LT_CLIENTIP, LT_ACTION,
                LT_MAPID, LT_X, LT_Y, LT_Z,
                LT_DWKEY1, LT_DWKEY2, LT_DWKEY3, LT_DWKEY4,
                LT_KEY1, LT_KEY2, LT_KEY3, LT_KEY4,
                LT_LOG
            FROM ' + QUOTENAME(@tbl) + N'
            WHERE NOT EXISTS (
                SELECT 1 FROM TLOG_AUDIT a
                WHERE a.LT_LOGDATE = ' + @date_expr + N'
                  AND a.LT_SERVERID = ' + QUOTENAME(@tbl) + N'.LT_SERVERID
                  AND a.LT_CLIENTIP = ' + QUOTENAME(@tbl) + N'.LT_CLIENTIP
                  AND a.LT_ACTION   = ' + QUOTENAME(@tbl) + N'.LT_ACTION
                  AND a.LT_DWKEY1   = ' + QUOTENAME(@tbl) + N'.LT_DWKEY1
            );';
    END

    BEGIN TRY
        EXEC sp_executesql @sql;
        SET @rows_one = @@ROWCOUNT;
        SET @rows_total = @rows_total + @rows_one;
        PRINT '  ' + @tbl + ': ' + CAST(@rows_one AS VARCHAR(20)) + ' rows';
    END TRY
    BEGIN CATCH
        PRINT '  ' + @tbl + ': SKIPPED (' + ERROR_MESSAGE() + ')';
    END CATCH

    FETCH NEXT FROM cur INTO @tbl;
END
CLOSE cur;
DEALLOCATE cur;

PRINT '';
PRINT 'Migrated ' + CAST(@rows_total AS VARCHAR(20))
    + ' row(s) from ' + CAST(@t_count AS VARCHAR(10))
    + ' source table(s) into TLOG_AUDIT.';
PRINT 'Source ITEMLOGTL* tables left intact — drop manually after verifying counts.';
GO
