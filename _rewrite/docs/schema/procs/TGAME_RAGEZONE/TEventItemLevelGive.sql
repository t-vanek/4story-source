
CREATE PROCEDURE [dbo].[TEventItemLevelGive]
	@szName 	VARCHAR(50),
	@wItemID	SMALLINT,
	@bLevel	TINYINT,
	@bCount 	TINYINT
AS

DECLARE 	@szTitle	 VARCHAR(50)
DECLARE 	@szMessage	 VARCHAR(100)
DECLARE 	@dwCharID	 INT
DECLARE 	@dwMakeID	 INT

SET @szTitle = '4s게임조선 이벤트 상품!'
SET @szMessage = '축하드립니다.'

SELECT @dwCharID=dwCharID FROM TCHARTABLE WHERE szName=@szName
IF @@ROWCOUNT = 0
BEGIN
	PRINT 'FAIL'
	RETURN 0
END

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

IF EXISTS(SELECT TOP 1 dwPostID FROM TPOSTTABLE WHERE dwPostID = @dwMakeID)
BEGIN
	INSERT INTO TPOSTITEMTABLE VALUES(
			@dwCharID,
			@dwMakeID,
			@wItemID,
			@bLevel,
			@bCount,0,
			0,0,0,0,0,0,
			0,0,0,0,0,0,
			0,0,0,0,0,0)
END



