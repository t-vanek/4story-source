
CREATE PROCEDURE [dbo].[TCheckAttend]
@dwUserID INT,
@dwCharID INT,
@szMessage VARCHAR(500) OUTPUT,
@bGiveItem TINYINT OUTPUT
AS

DECLARE @bDay TINYINT
DECLARE @bPrevDay TINYINT
DECLARE @bApply TINYINT
DECLARE @bContinue TINYINT

SET @szMessage = ''
SET @bPrevDay = 0
SET @bContinue = 0
SET @bGiveItem = 0

DECLARE CUR_ATTEND CURSOR FOR
SELECT bDay, bApply FROM TUSERATTENDTABLE WHERE dwUserID = @dwUserID
OPEN CUR_ATTEND
FETCH NEXT FROM CUR_ATTEND INTO @bDay, @bApply
WHILE @@FETCH_STATUS = 0
BEGIN

	SET @szMessage = @szMessage + CAST(@bDay AS VARCHAR(3)) + '  '

	IF(@bApply = 0)
	BEGIN
		IF(@bDay = @bPrevDay + 1)
			SET @bContinue = @bContinue + 1
		ELSE
			SET @bContinue = 1
	END
	ELSE
		SET @bContinue = 0

	IF(@bContinue = 3)
		SET @bGiveItem = 1

	SET @bPrevDay = @bDay

FETCH NEXT FROM CUR_ATTEND INTO @bDay, @bApply
END
CLOSE CUR_ATTEND
DEALLOCATE CUR_ATTEND

IF(@bGiveItem = 1)
BEGIN
	UPDATE TUSERATTENDTABLE SET bApply=1 WHERE dwUserID = @dwUserID
END

