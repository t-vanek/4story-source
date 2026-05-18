-- 2FA tables for TLoginSvrAsio. Lives in TGLOBAL_RAGEZONE alongside the
-- rest of the account / session metadata.
--
-- Apply: sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i 2fa-tables.sql

USE TGLOBAL_RAGEZONE;
GO

-- Per-user email + 2FA toggle. Separate from TUSERINFOTABLE so the
-- 2FA feature can be rolled out without touching the legacy schema's
-- shape. bTwoFactorEnabled = 1 turns on the new-device challenge.
IF OBJECT_ID('TUSEREMAIL', 'U') IS NULL
BEGIN
    CREATE TABLE TUSEREMAIL (
        dwUserID            INT          NOT NULL PRIMARY KEY,
        szEmail             VARCHAR(255) NOT NULL,
        bVerified           TINYINT      NOT NULL DEFAULT 0,
        bTwoFactorEnabled   TINYINT      NOT NULL DEFAULT 0,
        dUpdated            DATETIME     NOT NULL DEFAULT GETDATE()
    );
    PRINT 'Created TUSEREMAIL';
END
ELSE
    PRINT 'TUSEREMAIL already exists';
GO

-- Per-user whitelist of trusted client IPs. A login attempt from an
-- IP that's not on this list (when bTwoFactorEnabled = 1) triggers
-- the LR_SECURITY challenge; on a successful CS_SECURITYCONFIRM_ACK
-- the row is INSERTed. szIP is the dotted IPv4 the AsioSession
-- captured at accept time. The composite PK prevents duplicates.
IF OBJECT_ID('TUSERTRUSTEDIP', 'U') IS NULL
BEGIN
    CREATE TABLE TUSERTRUSTEDIP (
        dwUserID            INT          NOT NULL,
        szIP                VARCHAR(50)  NOT NULL,
        dAdded              DATETIME     NOT NULL DEFAULT GETDATE(),
        CONSTRAINT PK_TUSERTRUSTEDIP PRIMARY KEY (dwUserID, szIP)
    );
    CREATE INDEX IDX_TUSERTRUSTEDIP_dwUserID
        ON TUSERTRUSTEDIP(dwUserID);
    PRINT 'Created TUSERTRUSTEDIP';
END
ELSE
    PRINT 'TUSERTRUSTEDIP already exists';
GO
