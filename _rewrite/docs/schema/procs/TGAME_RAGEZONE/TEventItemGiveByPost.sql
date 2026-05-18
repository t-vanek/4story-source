CREATE PROCEDURE [dbo].[TEventItemGiveByPost]
@dwCharID INT,
@wItemID1 SMALLINT,
@bCount1 TINYINT,
@wItemID2 SMALLINT,
@bCount2 TINYINT,
@wItemID3 SMALLINT,
@bCount3 TINYINT,
@dwPeriod INT,
@szTitle VARCHAR(256),
@szMessage	VARCHAR(2048)
AS

DECLARE 	@dwPostID INT
DECLARE	@szTarget	VARCHAR(50)
DECLARE	@bLenTitle	BINARY(4)
DECLARE	@bLenMessage BINARY(4)
DECLARE 	@szT VARCHAR(8)
DECLARE	@szM VARCHAR(8)
DECLARE 	@bLevel TINYINT
DECLARE	@dEndDate SMALLDATETIME

SET @dEndDate = GETDATE() + @dwPeriod

SELECT @szTarget = szName, @bLevel = bLevel FROM TCHARTABLE WHERE dwCharID = @dwCharID

SET @bLenTitle = DATALENGTH(@szTitle)
SET @bLenMessage = DATALENGTH(@szMessage)
SET @szT = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenTitle), 8)
SET @szTitle  = @szT + @szTitle
SET @szM = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenMessage), 8)
SET @szMessage = @szM + @szMessage

INSERT INTO TPOSTTABLE(
	dwCharID,
	szSender,
	dwSendID,
	szRecvName,
	szTitle,
	szMessage,
	bType,
	bRead,
	dwGold,
	dwSilver,
	dwCooper,
	timeRecv) VALUES(
	@dwCharID,
	'[GM]',
	0,
	@szTarget,
	@szTitle,
	@szMessage,
	0,
	0,
	0,
	0,
	0,
	getdate())

SET @dwPostID = @@IDENTITY

DECLARE @bItemCount TINYINT
SET @bItemCount = 3

WHILE (@bItemCount > 0)
BEGIN
	DECLARE @dlID BIGINT
	DECLARE @dwDuraMax INT
	DECLARE @wInsertItem SMALLINT
	DECLARE @bInsertCount TINYINT

	IF(@bItemCount = 1)
	BEGIN
		SET @wInsertItem = @wItemID1
		SET @bInsertCount = @bCount1
	END
	ELSE IF(@bItemCount = 2)
	BEGIN
		SET @wInsertItem = @wItemID2
		SET @bInsertCount = @bCount2
	END
	IF(@bItemCount = 3)
	BEGIN
		SET @wInsertItem = @wItemID3
		SET @bInsertCount = @bCount3
	END

	IF(@wInsertItem = 0)
	BEGIN
		SET @bItemCount = @bItemCount - 1
		CONTINUE
	END

	SET @dwDuraMax = 0
	SELECT @dwDuraMax = dwDuraMax FROM TITEMCHART WHERE wItemID = @wInsertItem
	
	EXEC TGenerateDBItemID @dlID OUTPUT
	
	INSERT INTO TITEMTABLE(
			dlID,
			bStorageType,
			dwStorageID,
			bOwnerType,
			dwOwnerID,
			bItemID,
			wItemID,
			bLevel,
			bCount,
			bGLevel,
			dwDuraMax,
			dwDuraCur,
			bRefineCur,
			dEndTime,
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
			dwTime6,
			bGem,
			wMoggItemID) VALUES(
			@dlID,
			2,
			@dwPostID,
			0,
			@dwCharID,
			0,
			@wInsertItem,
			0,
			@bInsertCount,
			0,
			@dwDuraMax,
			@dwDuraMax,
			0,	--RefineCur
			@dEndDate,	--EndTime
			0,	--Magic1
			0,
			0,
			0,
			0,	
			0,
			0,	--Value1
			0,
			0,
			0,
			0,
			0,
			0,	--Time1
			0,
			0,
			0,
			0,
			0,
			0,
			0)

	SET @bItemCount = @bItemCount - 1
END

