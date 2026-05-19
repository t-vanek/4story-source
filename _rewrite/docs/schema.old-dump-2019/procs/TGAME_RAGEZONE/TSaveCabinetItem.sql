


CREATE PROCEDURE [dbo].[TSaveCabinetItem]
	@dwCharID	INT,
	@bCabinetID	TINYINT,
	@dwStItemID	INT,
	@wItemID	SMALLINT,
	@bLevel	TINYINT,
	@bCount	TINYINT,
	@bGLevel	TINYINT,
	@bMagic1	TINYINT,
	@bMagic2	TINYINT,
	@bMagic3	TINYINT,
	@bMagic4	TINYINT,
	@bMagic5	TINYINT,
	@bMagic6	TINYINT,
	@wValue1	SMALLINT,
	@wValue2	SMALLINT,
	@wValue3	SMALLINT,
	@wValue4	SMALLINT,
	@wValue5	SMALLINT,
	@wValue6	SMALLINT,
	@dwTime1	INT,
	@dwTime2	INT,
	@dwTime3	INT,
	@dwTime4	INT,
	@dwTime5	INT,
	@dwTime6	INT
AS

	BEGIN TRAN TSAVECABINETITEM

	INSERT INTO TTEMPCABINETITEMTABLE(
		dwCharID,
		bCabinetID,
		dwStItemID,
		wItemID,
		bLevel,
		bCount,
		bGLevel,
		bMagic1,
		bMagic2,
		bMagic3,
		bMagic4,
		bMagic5,
		bMagic6,
		wValue1,
		wValue2,
		wValue3,
		wValue4,
		wValue5,
		wValue6,
		dwTime1,
		dwTime2,
		dwTime3,
		dwTime4,
		dwTime5,
		dwTime6) VALUES(
		@dwCharID,
		@bCabinetID,
		@dwStItemID,
		@wItemID,
		@bLevel,
		@bCount,
		@bGLevel,
		@bMagic1,
		@bMagic2,
		@bMagic3,
		@bMagic4,
		@bMagic5,
		@bMagic6,
		@wValue1,
		@wValue2,
		@wValue3,
		@wValue4,
		@wValue5,
		@wValue6,
		@dwTime1,
		@dwTime2,
		@dwTime3,
		@dwTime4,
		@dwTime5,
		@dwTime6)

	COMMIT TRAN TSAVECABINETITEM
	RETURN 0



