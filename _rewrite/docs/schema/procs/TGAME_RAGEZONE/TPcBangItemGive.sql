



CREATE PROCEDURE [dbo].[TPcBangItemGive]
	@dwCharID 	INT
AS

DECLARE	@dlID		BIGINT
DECLARE	@dwUserID	INT
DECLARE 	@szTitle	 VARCHAR(256)
DECLARE 	@szMessage	 VARCHAR(2048)
DECLARE 	@szName	 VARCHAR(50)
DECLARE 	@dwMakeID	 INT
DECLARE	@bCount	TINYINT
DECLARE	@dwPcBangID	INT
DECLARE	@dCurDate	SMALLDATETIME
DECLARE	@dwRemainTime INT

SET @szTitle = 'PC방 아이템'
SET @szMessage = '부활 후유증 치료제'
SET @dwPcBangID = 0
SET @dCurDate = GetDate()
SET @dwRemainTime = 0

SELECT @dwUserID = dwUserID, @szName=szNAME FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF @@ROWCOUNT = 0
BEGIN
	RETURN 1
END
/*
SELECT @dwPcBangID = dwPcBangID FROM TGLOBAL_GSP.DBO.TCURRENTUSER WHERE dwUserID = @dwUserID
IF @@ROWCOUNT = 0
	RETURN 1

IF( @dwPcBangID = 0 )
BEGIN
	SELECT @dwRemainTime = dwRemainTime FROM TVIEW_DURINGITEMTABLE WHERE dwUserID = @dwUserID
	IF (@@ROWCOUNT = 0 OR @dwRemainTime = 0)
		RETURN 1
END
*/

SELECT @dwRemainTime = dwRemainTime FROM TVIEW_DURINGITEMTABLE WHERE dwUserID = @dwUserID AND wItemID IN(7401,7402,7403)
IF (@@ROWCOUNT = 0 OR @dwRemainTime = 0)
	RETURN 1

SELECT @bCount = bItemCnt FROM TGLOBAL_GSP.DBO.TPCBANGPLAYTABLE WHERE dwUserID = @dwUserID AND dwPlayDate = YEAR(@dCurDate)*10000 + MONTH(@dCurDate)*100 + DAY(@dCurDate)
IF (@@ROWCOUNT = 0 OR @bCount = 0)
	SET @bCount = 30
ELSE IF(@bCount < 30)
	SET @bCount = 30 - @bCount
ELSE
	SET @bCount = 0

IF(@bCount <> 0)
BEGIN
	EXEC TEventItemGive @szName, 7605, @bCount, @szTitle, @szMessage
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
	
	IF EXISTS(SELECT TOP 1 dwPostID FROM TPOSTTABLE WHERE dwPostID = @dwMakeID)
	BEGIN
		EXEC TGenerateDBItemID @dlID OUTPUT

		INSERT INTO TITEMTABLE(
			dlID,bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur,dEndtime,bGradeEffect,
			bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
			wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
			dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6) VALUES(
			@dlID,2,@dwMakeID, 0, @dwCharID,0,7605,0,@bCount,0,0,0,0,0,0,
			0,0,0,0,0,0,
			0,0,0,0,0,0,
			0,0,0,0,0,0)
	END
*/
END



