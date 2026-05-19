-- MSSQL dev schema — minimum tables to exercise SociAuthService
-- against SQL Server (prod target). Mirrors the equivalent rows from
-- schema/postgres-dev.sql but uses the original legacy types
-- (tinyint, smalldatetime, varchar) so this is byte-for-byte the
-- same DDL we'd ship to prod.
--
-- Apply:
--   tsql -H 127.0.0.1 -p 1433 -U sa -P 'DevPassword123!' < schema/mssql-dev.sql
--
-- Drop helpers up front so the script is idempotent.
USE tloginsvr_dev
go

IF OBJECT_ID('TLOG', 'U') IS NOT NULL DROP TABLE TLOG;
IF OBJECT_ID('TCURRENTUSER', 'U') IS NOT NULL DROP TABLE TCURRENTUSER;
IF OBJECT_ID('TUSERPROTECTED', 'U') IS NOT NULL DROP TABLE TUSERPROTECTED;
IF OBJECT_ID('TACCOUNT_PW', 'U') IS NOT NULL DROP TABLE TACCOUNT_PW;
IF OBJECT_ID('IPBLACKLIST_game', 'U') IS NOT NULL DROP TABLE IPBLACKLIST_game;
go

CREATE TABLE IPBLACKLIST_game (
    szIP varchar(50) NOT NULL PRIMARY KEY
);
go

CREATE TABLE TACCOUNT_PW (
    dwUserID    int          NOT NULL PRIMARY KEY,
    szUserID    varchar(50)  NOT NULL,
    szPasswd    varchar(255) NULL,
    bCheck      tinyint      NOT NULL DEFAULT 0,
    dFirstLogin smalldatetime NULL,
    dLastLogin  smalldatetime NULL
);
CREATE UNIQUE INDEX IDX_TACCOUNT_PW_szUserID ON TACCOUNT_PW(szUserID);
go

CREATE TABLE TUSERPROTECTED (
    dwSeq         int IDENTITY(1,1) PRIMARY KEY,
    dwUserID      int          NOT NULL,
    bBlockType    tinyint      NOT NULL,
    bEternal      tinyint      NOT NULL DEFAULT 0,
    startTime     smalldatetime NOT NULL DEFAULT GETDATE(),
    dwDuration    int          NOT NULL DEFAULT 0,
    bBlockReason  tinyint      NOT NULL DEFAULT 0,
    szComment     varchar(8000) NOT NULL DEFAULT ''
);
CREATE INDEX IDX_TUSERPROTECTED_dwUserID ON TUSERPROTECTED(dwUserID);
go

CREATE TABLE TCURRENTUSER (
    dwKEY        int IDENTITY(1,1) PRIMARY KEY,
    dwUserID     int          NOT NULL,
    dwCharID     int          NOT NULL DEFAULT 0,
    bGroupID     tinyint      NOT NULL DEFAULT 0,
    bChannel     tinyint      NOT NULL DEFAULT 0,
    szIPAddr     varchar(50)  NULL,
    wPort        smallint     NOT NULL DEFAULT 0,
    bLocked      tinyint      NOT NULL DEFAULT 0,
    dLoginDate   smalldatetime NOT NULL DEFAULT GETDATE(),
    dEnterDate   smalldatetime NOT NULL DEFAULT GETDATE(),
    szLoginIP    varchar(50)  NOT NULL DEFAULT '',
    bChanneling  tinyint      NOT NULL DEFAULT 0
);
CREATE INDEX IDX_TCURRENTUSER_dwUserID ON TCURRENTUSER(dwUserID);
go

CREATE TABLE TLOG (
    dwKEY       int          NOT NULL PRIMARY KEY,
    dwUserID    int          NOT NULL,
    dwCharID    int          NOT NULL DEFAULT 0,
    bGroupID    tinyint      NOT NULL DEFAULT 0,
    bChannel    tinyint      NOT NULL DEFAULT 0,
    timeLOGIN   smalldatetime NOT NULL DEFAULT GETDATE(),
    timeLOGOUT  smalldatetime NOT NULL DEFAULT GETDATE()
);
go

-- PC-Bang IP whitelist + premium tier — see postgres-dev.sql for the
-- prose. Optional: SociAuthService swallows "table missing" so a
-- deployment that doesn't care about either feature can skip them.
CREATE TABLE TPCBANG (
    szIP      varchar(40) NULL,
    szIPRange varchar(40) NULL,
    szName    varchar(64) NULL
);
CREATE INDEX IDX_TPCBANG_szIP ON TPCBANG(szIP);
go

CREATE TABLE TUSERPREMIUM (
    dwUserID    int          NOT NULL,
    dwPremiumID int          NOT NULL,
    dtStart     smalldatetime NULL,
    dtExpire    smalldatetime NOT NULL,
    PRIMARY KEY (dwUserID, dwPremiumID)
);
CREATE INDEX IDX_TUSERPREMIUM_user ON TUSERPREMIUM(dwUserID, dtExpire);
go
