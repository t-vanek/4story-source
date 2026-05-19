CREATE PROCEDURE [dbo].[TTESTGivePowerItem]
						@dwCharID INT,
						@wItemID SMALLINT,
						@bItemID TINYINT,
						@bEffect TINYINT,
						@bLevel TINYINT
AS

declare @bType TINYINT
declare @bKind TINYINT
declare @GenID BIGINT
declare @dwUserID INT

declare @bMagic1 TINYINT
declare @bMagic2 TINYINT
declare @bMagic3 TINYINT
declare @bMagic4 TINYINT
declare @bMagic5 TINYINT
declare @bMagic6 TINYINT
declare @wValue1 SMALLINT
declare @wValue2 SMALLINT
declare @wValue3 SMALLINT
declare @wValue4 SMALLINT
declare @wValue5 SMALLINT
declare @wValue6 SMALLINT
declare @dwTime1 INT

SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID

SELECT @bType = bType, @bKind = bKind, @dwTime1 =  blevel FROM TITEMCHART WHERE wItemID = @wItemID

SELECT @GenID = dlID FROM TITEMTABLE WHERE dwOwnerID = @dwCharID AND bOwnerType = 0 AND bStorageType = 0 AND dwStorageID = 254 AND bItemID = @bItemID


  EXEC TGenerateDBItemID @GenID OUTPUT


BEGIN
  insert TITEMTABLE (dlID, bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,

	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6, 
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
  values(@GenID,0,254,0,@dwCharID, @bItemID,@wItemID,@bLevel,1,0,9999,9999,0,0,2,
	0,0,3,0,0,0, 
	0,0,5461,0,0,0, 
	0,0,0,0,0,0, 0, 0)
END

