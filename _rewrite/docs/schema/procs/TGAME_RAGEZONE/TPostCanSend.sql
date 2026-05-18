

CREATE PROCEDURE [dbo].[TPostCanSend]
@dwCharID INT OUTPUT,
@szName VARCHAR(50),
@szTarget VARCHAR(50),
@bType TINYINT
AS

DECLARE @dwCount INT
SET @dwCount = 0

DECLARE @dwID INT
SET @dwID = 0

DECLARE @bCountry TINYINT
DECLARE @bTargetCountry TINYINT
DECLARE @dwUserID INT

SELECT @dwCharID = dwCharID, @bTargetCountry = bCountry, @dwUserID = dwUserID FROM TCHARTABLE WHERE szName = @szTarget AND bDelete = 0
IF(@@ROWCOUNT = 0)
	RETURN 1

SELECT @dwID = dwCharID, @bCountry = bCountry FROM TCHARTABLE WHERE szName = @szName
IF(@@ROWCOUNT = 0)
	RETURN 1

IF(@bTargetCountry < 2 AND @bCountry < 2 AND @bTargetCountry <> @bCountry)
	RETURN 1
/*
IF EXISTS(SELECT dwUserID FROM TCHARTABLE WHERE dwCharID=@dwID AND dwUserID = @dwUserID)
	RETURN 14
*/
IF(@bType < 3)
BEGIN
	SELECT @dwCount = COUNT(dwPostID) FROM TPOSTTABLE WHERE dwCharID = @dwCharID
	IF(@dwCount >= 20)
		RETURN 6
END

SELECT dwCharID FROM TPROTECTEDTABLE WHERE dwCharID = @dwCharID AND dwProtected = @dwID
IF(@@ROWCOUNT != 0)
	RETURN 1

RETURN 0


