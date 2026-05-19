



CREATE PROCEDURE [dbo].[TSaveSMS]
@bWorld TINYINT,
@bType TINYINT,
@dwSenderID INT,
@dwGuildID INT,
@szTarget VARCHAR(50),
@szMessage VARCHAR(80)
AS

DECLARE @szName VARCHAR(50)
DECLARE @dwUserID INT
DECLARE @dateSend SMALLDATETIME

SELECT @szName = szNAME FROM TCHARTABLE WHERE dwCharID = @dwSenderID
IF(@@ROWCOUNT <> 1)
	RETURN 4

SET @dateSend = GetDate()

IF(@bType = 1)
BEGIN
	IF EXISTS(SELECT dwCharID FROM TCHARTABLE WHERE szNAME = @szTarget)
		INSERT INTO TGLOBAL_GSP.DBO.TSMSTABLE(bWorld, dwUserID, szCharName, bType, szSender, szMessage, dateSend) 
						   SELECT @bWorld, dwUserID, szNAME, @bType, @szName, @szMessage, @dateSend FROM TCHARTABLE WHERE szNAME = @szTarget
	ELSE
		RETURN 4
END
ELSE
BEGIN
	IF EXISTS(SELECT dwID FROM TGUILDTABLE WHERE dwID = @dwGuildID)
		INSERT INTO TGLOBAL_GSP.DBO.TSMSTABLE(bWorld, dwUserID, szCharName, bType, szSender, szMessage, dateSend) 
		   	 SELECT @bWorld, TCHARTABLE.dwUserID, TCHARTABLE.szNAME, @bType, @szName, @szMessage, @dateSend FROM TCHARTABLE INNER JOIN TGUILDMEMBERTABLE ON TCHARTABLE.dwCharID = TGUILDMEMBERTABLE.dwCharID
				WHERE TGUILDMEMBERTABLE.dwGuildID = @dwGuildID
	ELSE
		RETURN 4
END

RETURN @@ERROR



