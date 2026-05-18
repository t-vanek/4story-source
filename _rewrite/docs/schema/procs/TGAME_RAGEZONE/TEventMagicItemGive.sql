


CREATE PROCEDURE [dbo].[TEventMagicItemGive]
	@dwCharID 	INT,
	@bCount 	TINYINT
AS

DECLARE 	@szTitle	 VARCHAR(50)
DECLARE 	@szMessage	 VARCHAR(100)
DECLARE 	@szName	 VARCHAR(50)
DECLARE 	@dwMakeID	 INT

SET @szTitle = '이벤트 퀘스트 보상'
SET @szMessage = '축하드립니다.'

SELECT @szName=szNAME FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF @@ROWCOUNT = 0
BEGIN
	RETURN 1
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
			7211,
			0,
			@bCount,0,
			3,0,0,0,0,0,
			23,0,0,0,0,0,
			0,0,0,0,0,0)
END




