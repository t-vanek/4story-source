
CREATE PROCEDURE [dbo].[TAgreement]
@dwUserID INT
AS

IF EXISTS(SELECT dwUserID FROM TUSERINFOTABLE WHERE dwUserID = @dwUserID)
	UPDATE TUSERINFOTABLE SET bAgreement = bAgreement + 1 WHERE dwUserID = @dwUserID
ELSE
	INSERT INTO TUSERINFOTABLE (dwUserID, bAgreement,dCabinetUse) VALUES(@dwUserID , 1,'2008-01-01')

DECLARE @dReleaseDate SMALLDATETIME
SELECT @dReleaseDate = dReleaseDate FROM releaseDate

DECLARE @dCheckTime SMALLDATETIME
SET @dCheckTime = DATEADD("d", 7, @dReleaseDate)

IF(CONVERT(date, GETDATE()) < @dCheckTime)
BEGIN
	UPDATE TUSERINFOTABLE SET dCabinetUse = DATEADD("d", 360, GETDATE()) WHERE dwUserID = @dwUserID

	/*IF EXISTS(SELECT dwUserID FROM TCASHTESTTABLE WHERE dwUserID = @dwUserID)
	 UPDATE TCASHTESTTABLE SET dwCash = dwCash + 500 WHERE dwUserID = @dwUserID
	ELSE
	 INSERT INTO TCASHTESTTABLE VALUES(@dwUserID, 500, 0, null, null)*/
	
	--EXEC TGAME_GSP.dbo.TItemInsert @dwUserID, 18444, 5

	DECLARE @dwENDTIME SMALLDATETIME
	SET @dwENDTIME = (DATEADD("d", 30, GETDATE()))

	INSERT INTO TCASHITEMCABINETTABLE(
		dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
		VALUES(@dwUserID, 18428, 0, 1, 0, 0, 0, 0, @dwENDTIME, 0,
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0)
END

