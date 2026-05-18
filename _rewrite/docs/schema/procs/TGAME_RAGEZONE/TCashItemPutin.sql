


CREATE PROCEDURE [dbo].[TCashItemPutin]
@dwNewID INT OUTPUT,
@dwCash INT OUTPUT,
@dwBonus INT OUTPUT,
@dwCharID INT,
@bInven TINYINT,
@bItemID TINYINT
AS

DECLARE @dwReturn INT
DECLARE @dwUserID INT
DECLARE @dUseTime SMALLDATETIME
DECLARE @wItemID SMALLINT
DECLARE @bWorldID TINYINT
DECLARE @bLevel TINYINT
DECLARE @bRefine TINYINT
DECLARE @bCount TINYINT
DECLARE @dlID BIGINT

SET @dwReturn = 1
SET @dwNewID= 0
SET @bWorldID = 0

SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0)
	RETURN 4

/*SELECT @bCount = COUNT(*) FROM TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE WHERE dwUserID = @dwUserID
IF(@bCount > 10)
	RETURN 4*/

SELECT @dlID=dlID, @wItemID = wItemID, @bLevel=bLevel, @bRefine=bRefineCur FROM TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID=@bInven AND bItemID = @bItemID
IF(@@ROWCOUNT = 0)
	RETURN 5

IF(@bLevel > 0 OR  @bRefine > 0)
	SELECT @bWorldID = bWorld+1 FROM TDBITEMINDEXTABLE

INSERT INTO  TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE(
	dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID,
	bWorldID, dlID)
SELECT @dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime,bGradeEffect,
	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID,
	@bWorldID, @dlID
 FROM TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID=@bInven AND bItemID = @bItemID

SET @dwNewID = @@IDENTITY

DELETE TITEMTABLE WHERE dwOwnerID=@dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID=@bInven AND bItemID = @bItemID


