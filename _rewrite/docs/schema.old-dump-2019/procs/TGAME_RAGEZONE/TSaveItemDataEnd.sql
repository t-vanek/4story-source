



CREATE PROCEDURE [dbo].[TSaveItemDataEnd]
@dwCharID INT
AS

BEGIN TRAN SAVEITEMEND

DELETE TINVENTABLE WHERE dwCharID = @dwCharID
DELETE TITEMTABLE WHERE dwOwnerID = @dwCharID AND bOwnerType=0 AND bStorageType=0
DELETE TITEMTABLE WHERE dlID IN(SELECT dlID FROM TTEMPITEMTABLE WHERE  dwOwnerID = @dwCharID AND bOwnerType=0 AND bStorageType=0)

INSERT INTO TINVENTABLE SELECT * FROM TTEMPINVENTABLE WHERE dwCharID = @dwCharID
INSERT INTO TITEMTABLE(
	dlID, bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur,dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
	SELECT * FROM TTEMPITEMTABLE WHERE  dwOwnerID = @dwCharID AND bOwnerType=0 AND bStorageType=0

COMMIT TRAN SAVEITEMEND


