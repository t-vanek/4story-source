
CREATE PROCEDURE [dbo].[TUpdateActiveChar]
@dwCharID INT
AS

DECLARE @bLevel TINYINT
DECLARE @bCountry TINYINT
DECLARE @bAidCountry TINYINT
DECLARE @dCurDate SMALLDATETIME

SET @bCountry = 3
SET @bAidCountry = 3
SET @dCurDate = GetDate()

SELECT @bLevel=bLevel, @bCountry=bCountry FROM TCHARTABLE WHERE dwCharID = @dwCharID
SELECT @bAidCountry = bCountry FROM TAIDTABLE WHERE dwCharID = @dwCharID

IF EXISTS( SELECT dwCharID FROM TACTIVECHARTABLE WHERE dwCharID = @dwCharID)
BEGIN
	IF(@bCountry < 2 OR @bAidCountry < 2)
		UPDATE TACTIVECHARTABLE SET dateEnter = @dCurDate WHERE dwCharID=@dwCharID
	ELSE
		DELETE TACTIVECHARTABLE WHERE dwCharID=@dwCharID
END
ELSE IF(@bLevel >= 80 AND (@bCountry < 2 OR @bAidCountry < 2))
	INSERT INTO TACTIVECHARTABLE (dwCharID, dateEnter) VALUES(@dwCharID, @dCurDate)


