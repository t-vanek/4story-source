
CREATE PROCEDURE [dbo].[TCMGiftSet]
	@wGiftID		SMALLINT,
	@bGiftType 		TINYINT,
	@dwValue		INT,
	@bCount		TINYINT,
	@bTakeType 		TINYINT, 
	@bMaxTakeCount	TINYINT,
	@bLevel 		TINYINT,
	@bToolOnly 		TINYINT,
	@wErrGiftID		SMALLINT,
	@szTitle 		VARCHAR(50),
	@szMsg 		VARCHAR(1024)
AS

--UPDATE
IF EXISTS(SELECT wGiftID FROM TCMGIFTCHART WHERE wGiftID = @wGiftID)
BEGIN
	UPDATE TCMGIFTCHART SET bGiftType = @bGiftType, dwValue = @dwValue, bCount = @bCount, 
			bTakeType = @bTakeType, bMaxTakeCount = @bMaxTakeCount, bLevel = @bLevel, bToolOnly = @bToolOnly,
			wErrGiftID = @wErrGiftID, szTitle = @szTitle, szMsg = @szMsg
	WHERE wGiftID = @wGiftID
END


