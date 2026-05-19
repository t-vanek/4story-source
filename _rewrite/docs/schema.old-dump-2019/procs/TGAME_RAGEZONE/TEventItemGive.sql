


CREATE PROCEDURE [dbo].[TEventItemGive]
	@szName 	VARCHAR(50),
	@wItemID	SMALLINT,
	@bCount 	TINYINT,
	@szTitle	VARCHAR(256),
	@szMessage	VARCHAR(2048)
AS

DECLARE 	@dlID		BIGINT
DECLARE 	@dwCharID	 INT
DECLARE 	@dwMakeID	 INT
DECLARE	@bLenTitle	BINARY(4)
DECLARE	@bLenMessage BINARY(4)
DECLARE @szT VARCHAR(8)
DECLARE @szM VARCHAR(8)

SELECT @dwCharID=dwCharID FROM TCHARTABLE WHERE szName=@szName AND bDelete = 0
IF @@ROWCOUNT = 0
BEGIN
	PRINT 'FAIL'
	RETURN 0
END

SET @bLenTitle = DATALENGTH(@szTitle)
SET @bLenMessage = DATALENGTH(@szMessage)
SET @szT = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenTitle), 8)
SET @szTitle  = @szT + @szTitle
SET @szM = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenMessage), 8)
SET @szMessage = @szM + @szMessage

DECLARE @dwDuraMax INT
SET @dwDuraMax = 0
SELECT @dwDuraMax = dwDuraMax FROM TITEMCHART WHERE wItemID = @wItemID

INSERT INTO TRESERVEDPOST(
	dwRecverID,
	szSender,
	szTitle,
	szMessage,
	bSend,
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
	@dwCharID,
	'4StoryPW Team',
	@szTitle,
	@szMessage,
	0,
	@wItemID,
	0,
	@bCount,
	0,
	@dwDuraMax,
	@dwDuraMax,
	0,
	0,
	0,0,0,0,0,0,
	0,0,0,0,0,0,
	0,0,0,0,0,0,0,0)

/*
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
	'운영자',
	0,
	@szName,
	@szTitle,
	@szMessage,
	1,
	0,
	0,
	0,
	0,
	GetDate())

SET @dwMakeID = @@IDENTITY

DECLARE @dwDuraMax INT
SET @dwDuraMax = 0
SELECT @dwDuraMax = dwDuraMax FROM TITEMCHART WHERE wItemID = @wItemID

IF EXISTS(SELECT TOP 1 dwPostID FROM TPOSTTABLE WHERE dwPostID = @dwMakeID)
BEGIN

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
		dwTime6) VALUES(
		@dlID,
		2,
		@dwMakeID,
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
		0,	--EndTime
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
		0)
END

RETURN @dwMakeID
*/


