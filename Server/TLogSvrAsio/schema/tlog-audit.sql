-- Modern audit log table for TLogSvrAsio. Single table with
-- LT_LOGDATE indexed, replacing the legacy date-partitioned
-- ITEMLOGTLyyyymmdd schema (the partitioned approach forced an
-- external job to pre-create each day's table, and SQL Server has
-- proper partitioning built in if a deploy actually needs it).
--
-- Apply: sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i tlog-audit.sql

USE TGLOBAL_RAGEZONE;
GO

IF OBJECT_ID('TLOG_AUDIT', 'U') IS NULL
BEGIN
    CREATE TABLE TLOG_AUDIT (
        LT_ID         BIGINT IDENTITY(1,1) PRIMARY KEY,
        LT_LOGDATE    DATETIME      NOT NULL DEFAULT GETDATE(),
        LT_SERVERID   INT           NOT NULL,
        LT_CLIENTIP   VARCHAR(16)   NOT NULL,
        LT_ACTION     INT           NOT NULL,
        LT_MAPID      SMALLINT      NOT NULL DEFAULT 0,
        LT_X          INT           NOT NULL DEFAULT 0,
        LT_Y          INT           NOT NULL DEFAULT 0,
        LT_Z          INT           NOT NULL DEFAULT 0,
        -- 11 search-key ints + 7 search-key strings + format byte
        LT_DWKEY1     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY2     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY3     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY4     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY5     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY6     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY7     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY8     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY9     BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY10    BIGINT        NOT NULL DEFAULT 0,
        LT_DWKEY11    BIGINT        NOT NULL DEFAULT 0,
        LT_KEY1       VARCHAR(50)   NOT NULL DEFAULT '',
        LT_KEY2       VARCHAR(50)   NOT NULL DEFAULT '',
        LT_KEY3       VARCHAR(50)   NOT NULL DEFAULT '',
        LT_KEY4       VARCHAR(50)   NOT NULL DEFAULT '',
        LT_KEY5       VARCHAR(50)   NOT NULL DEFAULT '',
        LT_KEY6       VARCHAR(50)   NOT NULL DEFAULT '',
        LT_KEY7       VARCHAR(50)   NOT NULL DEFAULT '',
        LT_FMT        INT           NOT NULL DEFAULT 0,
        LT_LOG        VARBINARY(512) NULL
    );

    CREATE INDEX IDX_TLOG_AUDIT_LogDate ON TLOG_AUDIT(LT_LOGDATE);
    CREATE INDEX IDX_TLOG_AUDIT_Action  ON TLOG_AUDIT(LT_ACTION);
    CREATE INDEX IDX_TLOG_AUDIT_DwKey1  ON TLOG_AUDIT(LT_DWKEY1);
    PRINT 'Created TLOG_AUDIT';
END
ELSE
    PRINT 'TLOG_AUDIT already exists';
GO
