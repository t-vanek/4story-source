CREATE PROCEDURE [dbo].[TUserProtectedAdd]
@szUsername varchar(51),
@dwDuration int,
@szReason varchar(51),
@bPermanent TINYINT,
@szOperator varchar(51)
AS

DECLARE @dwUserID INT
DECLARE @dwCharID INT

SELECT @dwUserID = dwUserID, @dwCharID = dwCharID FROM TGAME_GSP.dbo.TCHARTABLE WHERE szNAME = @szUsername

IF (@@ROWCOUNT = 0)
BEGIN
	/*Character not found*/
	RETURN 0
END

INSERT INTO TUSERPROTECTED(dwUserID, bBlockType, bEternal, bWorld, dwCharID, szCharName, startTime, dwDuration, bBlockReason, szComment, szGMID, regDate, sentBanMail)
VALUES(@dwUserID, 1, @bPermanent, 1, @dwCharID, @szUsername, getdate(), @dwDuration, 1, @szReason, @szOperator, getdate(), 0)

/*Success*/
RETURN 1
