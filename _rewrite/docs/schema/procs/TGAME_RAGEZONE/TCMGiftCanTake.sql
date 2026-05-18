
CREATE PROCEDURE [dbo].[TCMGiftCanTake]
	@szTarget 	VARCHAR(50),
	@wGiftID	SMALLINT
AS

DECLARE @dwUserID 		INT
DECLARE @dwCharID 		INT
DECLARE @bGiftType 	TINYINT
DECLARE @bTakeType 	TINYINT
DECLARE @bMaxTakeCount TINYINT
DECLARE @wTempCount SMALLINT

-- FindChar
SELECT @dwUserID = dwUserID, @dwCharID = dwCharID FROM TCHARTABLE WHERE szNAME = @szTarget
IF(@@ROWCOUNT= 0)
	RETURN 1

--FindGift
SELECT @bTakeType = bTakeType, @bMaxTakeCount = bMaxTakeCount FROM TCMGIFTCHART WHERE wGiftID = @wGiftID
IF(@@ROWCOUNT= 0)
	RETURN 2

--CheckCount
IF(@bTakeType = 1) --Account CountCheck
BEGIN
	SELECT @wTempCount = count(dwUserID) FROM TCMGIFTTABLE WHERE dwUserID = @dwUserID AND wGiftID = @wGiftID
	IF(@bMaxTakeCount != 0 AND @wTempCount >= @bMaxTakeCount)
		RETURN 3
END

ELSE IF(@bTakeType = 2) --Charactor CountCheck
BEGIN
	SELECT @wTempCount = count(dwCharID) FROM TCMGIFTTABLE WHERE dwCharID = @dwCharID AND wGiftID = @wGiftID
	IF(@bMaxTakeCount != 0 AND @wTempCount >= @bMaxTakeCount)
		RETURN 3
END

RETURN 0


