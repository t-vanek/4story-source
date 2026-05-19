
CREATE PROCEDURE [dbo].[TCMGiftAdd]
	@wGiftID		INT	OUTPUT,
	@bGiftType 		TINYINT,
	@dwValue		INT,
	@bCount		TINYINT,
	@bTakeType 		TINYINT, 
	@bMaxTakeCount	TINYINT,
	@bLevel		TINYINT,
	@bToolOnly 		TINYINT,
	@wErrGiftID		SMALLINT,
	@szTitle 		VARCHAR(50),
	@szMsg 		VARCHAR(1024)
AS
--SELECT @wGiftID = MAX(wGiftID)+1 FROM TCMGIFTCHART
INSERT TCMGIFTCHART (bGiftType, dwValue, bCount, bTakeType, bMaxTakeCount, bLevel, bToolOnly, wErrGiftID, szTitle, szMsg)
VALUES(@bGiftType, @dwValue, @bCount, @bTakeType, @bMaxTakeCount, @bLevel, @bToolOnly, @wErrGiftID, @szTitle, @szMsg)
SET @wGiftID = @@IDENTITY


