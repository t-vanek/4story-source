
CREATE PROCEDURE [dbo].[Test]

AS
	DECLARE @nTest	INT
	DECLARE @nCount	INT
	DECLARE @dwID 	INT

SET @nCount = 800

WHILE @nTest < @nCount
BEGIN

	INSERT INTO TCASHGAMBLECHART (dwID, wItemID, bCount, dwProb, bLevel, bGLevel, bGradeEffect, bMagic1, wValue1, dwTime1, bMagic2, wValue2, dwTime2 , bMagic3, wValue3, dwTime3, bMagic4, wValue4, dwTime4, bMagic5, wValue5, dwTime5, bMagic6, wValue6, dwTime6, wGroup, wUseTime, dwDuraMax, dwDuraCur, bRefineCur) VALUES(@nTest,7556,1,100,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
	SET @nTest += 1 
END


	

