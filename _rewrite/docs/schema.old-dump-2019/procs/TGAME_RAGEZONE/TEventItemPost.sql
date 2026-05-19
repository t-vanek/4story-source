
CREATE  PROCEDURE [dbo].[TEventItemPost]
@dwCharID INT,
@wItemID SMALLINT,
@bCount TINYINT,
@szTitle	VARCHAR(256),
@szMessage	VARCHAR(2048)
AS

DECLARE 	@dwPostID INT
DECLARE	@szTarget	VARCHAR(50)
DECLARE	@bLenTitle	BINARY(4)
DECLARE	@bLenMessage BINARY(4)
DECLARE 	@szT VARCHAR(8)
DECLARE	@szM VARCHAR(8)
DECLARE 	@bLevel TINYINT
DECLARE 	@dEndDate SMALLDATETIME
DECLARE @dlID BIGINT
DECLARE @dwDuraMax INT

SET @dwDuraMax = 0
SELECT @dwDuraMax = dwDuraMax FROM TITEMCHART WHERE wItemID = @wItemID
IF (@wItemID > 0 AND @@ROWCOUNT = 0)
BEGIN
	PRINT 'There is no Item'
	RETURN 1
END

SELECT @szTarget = szName, @bLevel = bLevel FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF (@@ROWCOUNT = 0 OR @szTarget is null)
BEGIN
	PRINT 'There is no Char'
	RETURN 2
END

SET @bLenTitle = DATALENGTH(@szTitle)
SET @bLenMessage = DATALENGTH(@szMessage)
SET @szT = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenTitle), 8)
SET @szTitle  = @szT + @szTitle
SET @szM = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenMessage), 8)
SET @szMessage = @szM + @szMessage
SET @dEndDate = 0

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
		@wItemID,
		0,
		@bCount,
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


