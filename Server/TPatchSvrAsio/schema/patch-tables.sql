-- MSSQL dev schema for TPatchSvrAsio. Three patch-metadata tables
-- (TVERSION, TPREVERSION, TUSER_INTERFACE) plus the two stored
-- procedures the legacy server calls (TMinBetaVer + TPreCompleteAdd).
-- Mirrors the column types in `_rewrite/docs/schema/TGLOBAL_RAGEZONE.
-- tables.csv` byte-for-byte so this same DDL applies to the prod
-- RageZone DB.
--
-- Apply:
--   sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i patch-tables.sql
-- or, for a dev DB:
--   sqlcmd -S localhost -U sa -P 'DevPassword123!' -d tpatchsvr_dev \
--          -i patch-tables.sql
--
-- Idempotent: tables created only if missing; procs are dropped and
-- recreated unconditionally so editing this file is the canonical
-- way to update them.

------------------------------------------------------------------------
-- Tables
------------------------------------------------------------------------

IF OBJECT_ID('TVERSION', 'U') IS NULL
BEGIN
    CREATE TABLE TVERSION (
        dwVersion  INT          NOT NULL,
        szPath     VARCHAR(260) NOT NULL,
        szName     VARCHAR(260) NOT NULL,
        dwSize     INT          NOT NULL DEFAULT 0,
        dwBetaVer  INT          NULL,
        CONSTRAINT PK_TVERSION PRIMARY KEY CLUSTERED (dwVersion)
    );
    PRINT 'Created TVERSION';
END
ELSE
    PRINT 'TVERSION already exists';
GO

IF OBJECT_ID('TPREVERSION', 'U') IS NULL
BEGIN
    CREATE TABLE TPREVERSION (
        dwBetaVer  INT          NOT NULL,
        szPath     VARCHAR(260) NOT NULL,
        szName     VARCHAR(260) NOT NULL,
        dwSize     INT          NOT NULL DEFAULT 0,
        CONSTRAINT PK_TPREVERSION PRIMARY KEY CLUSTERED (dwBetaVer)
    );
    PRINT 'Created TPREVERSION';
END
ELSE
    PRINT 'TPREVERSION already exists';
GO

-- TUSER_INTERFACE keys interface/UI files to an "option byte" the
-- client carries in CT_CHANGEIF_REQ. dwSize is FLOAT in the legacy
-- schema (and the modern repo binds it as DOUBLE) — preserve that
-- exactly so the wire layer never sees a narrowing surprise.
IF OBJECT_ID('TUSER_INTERFACE', 'U') IS NULL
BEGIN
    CREATE TABLE TUSER_INTERFACE (
        bOption    TINYINT      NOT NULL,
        szName     VARCHAR(260) NOT NULL,
        dwSize     FLOAT        NOT NULL DEFAULT 0,
        CONSTRAINT PK_TUSER_INTERFACE PRIMARY KEY CLUSTERED (bOption, szName)
    );
    PRINT 'Created TUSER_INTERFACE';
END
ELSE
    PRINT 'TUSER_INTERFACE already exists';
GO

------------------------------------------------------------------------
-- Stored procedures
------------------------------------------------------------------------

-- TMinBetaVer — operator-configured cutoff returned in
-- CT_NEWPATCH_ACK.dwMinBetaVer. Default value of 2 matches what the
-- restored RageZone DB ships; operators tune this in production.
IF OBJECT_ID('dbo.TMinBetaVer', 'P') IS NOT NULL
    DROP PROCEDURE dbo.TMinBetaVer;
GO
CREATE PROCEDURE dbo.TMinBetaVer
    @dwMinVer INT OUTPUT
AS
BEGIN
    SET @dwMinVer = 2
END
GO
PRINT 'Created/updated dbo.TMinBetaVer';
GO

-- TPreCompleteAdd — promotes every TPREVERSION row at a given beta
-- into TVERSION and removes the pre-version rows. Legacy referenced
-- this SP from CT_PREPATCHCOMPLETE_REQ but the restored DB never
-- shipped a body; this implementation matches what operators were
-- doing by hand. Modern's PatchRepository::MarkPreVersionComplete
-- runs the same MERGE+DELETE sequence inline (so the SP being
-- absent isn't fatal), but having the SP deployed lets legacy
-- TPatchSvr binaries call it directly.
IF OBJECT_ID('dbo.TPreCompleteAdd', 'P') IS NOT NULL
    DROP PROCEDURE dbo.TPreCompleteAdd;
GO
CREATE PROCEDURE dbo.TPreCompleteAdd
    @dwBetaVer INT
AS
BEGIN
    SET NOCOUNT ON;
    BEGIN TRANSACTION;

    -- Upsert pre-versions into TVERSION at (szPath, szName).
    MERGE INTO TVERSION AS T
    USING (SELECT dwBetaVer, szPath, szName, dwSize
           FROM TPREVERSION
           WHERE dwBetaVer = @dwBetaVer) AS S
    ON  T.szPath = S.szPath
    AND T.szName = S.szName
    WHEN MATCHED THEN UPDATE SET
        T.dwVersion = S.dwBetaVer,
        T.dwSize    = S.dwSize,
        T.dwBetaVer = S.dwBetaVer
    WHEN NOT MATCHED THEN INSERT
        (dwVersion, szPath, szName, dwSize, dwBetaVer)
        VALUES (S.dwBetaVer, S.szPath, S.szName, S.dwSize, S.dwBetaVer);

    -- Remove now-promoted rows.
    DELETE FROM TPREVERSION WHERE dwBetaVer = @dwBetaVer;

    COMMIT TRANSACTION;
    RETURN @@ERROR;
END
GO
PRINT 'Created/updated dbo.TPreCompleteAdd';
GO

------------------------------------------------------------------------
-- Optional seed rows (commented; uncomment for an empty dev DB)
------------------------------------------------------------------------
-- INSERT INTO TVERSION (dwVersion, szPath, szName, dwSize, dwBetaVer)
-- VALUES (1, '\\\\patch\\', 'client.exe', 102400, NULL);
-- INSERT INTO TPREVERSION (dwBetaVer, szPath, szName, dwSize)
-- VALUES (3, '\\\\patch\\beta\\', 'client.exe', 110000);
-- INSERT INTO TUSER_INTERFACE (bOption, szName, dwSize)
-- VALUES (0, 'ui_default.dat', 4096);
