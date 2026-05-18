-- Insert a developer account against the restored TGLOBAL_RAGEZONE
-- so the modernized login flow can be exercised end-to-end.
--
-- Credentials:  dev / dev123
-- BCrypt hash:  $2b$10$uhOJVoD7ylkykDr5ZNp.F.P12ejOlRQoLlDQQTKhyAeTvnQ1yp8vC
--
-- Apply: sqlcmd -S localhost -E -d TGLOBAL_RAGEZONE -i dev-account.sql

USE TGLOBAL_RAGEZONE;
GO

DECLARE @uid INT;

IF EXISTS (SELECT 1 FROM TACCOUNT_PW WHERE szUserID = 'dev')
BEGIN
    SELECT @uid = dwUserID FROM TACCOUNT_PW WHERE szUserID = 'dev';
    UPDATE TACCOUNT_PW
       SET szPasswd    = '$2b$10$uhOJVoD7ylkykDr5ZNp.F.P12ejOlRQoLlDQQTKhyAeTvnQ1yp8vC',
           dLastLogin  = GETDATE()
     WHERE dwUserID = @uid;
    PRINT CONCAT('Updated dev account, dwUserID=', @uid);
END
ELSE
BEGIN
    INSERT INTO TACCOUNT_PW
        (szUserID, szPasswd, bCheck, dFirstLogin, dLastLogin)
    VALUES
        ('dev',
         '$2b$10$uhOJVoD7ylkykDr5ZNp.F.P12ejOlRQoLlDQQTKhyAeTvnQ1yp8vC',
         0, GETDATE(), GETDATE());
    SET @uid = SCOPE_IDENTITY();
    PRINT CONCAT('Created dev account, dwUserID=', @uid);
END

-- Ensure the agreement row exists with bAgreement=1 so the login
-- doesn't return LR_NEEDAGREEMENT on the first attempt.
IF EXISTS (SELECT 1 FROM TUSERINFOTABLE WHERE dwUserID = @uid)
BEGIN
    UPDATE TUSERINFOTABLE
       SET bCanCreateCharCount = 6,
           bAgreement = 1
     WHERE dwUserID = @uid;
END
ELSE
BEGIN
    INSERT INTO TUSERINFOTABLE (dwUserID, bCanCreateCharCount, bAgreement, dCabinetUse)
    VALUES (@uid, 6, 1, GETDATE());
END

-- Clear any stale TCURRENTUSER row from a previous test run so the
-- next login doesn't immediately hit LR_DUPLICATE.
DELETE FROM TCURRENTUSER WHERE dwUserID = @uid;

PRINT 'dev account ready (uid printed above), password = dev123';
GO
