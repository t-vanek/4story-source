
/*
	RETURN VALUE
	0 : SUCCESS 
	1 : Invalid character
	2:  Invalid Quest

*/
CREATE PROCEDURE [dbo].[TQuestSendPost]
	@dwQuestID	INT,
	@dwCharID	INT,	
	@wItemID	SMALLINT,
	@bLevel	TINYINT,
	@bCount	TINYINT,
	@bGLevel	TINYINT,
	@dwDuraMax	INT,
	@dwDuraCur	INT,
	@bRefineCur	TINYINT,
	@dEndTime	SMALLDATETIME,
	@bGradeEffect 	TINYINT,
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
	DECLARE @szTitle	VARCHAR(255)
	DECLARE @szMessage	VARCHAR(1024)
	DECLARE @bLenTitle	BINARY(4)
	DECLARE @bLenMessage BINARY(4)
	DECLARE @szT VARCHAR(8)
	DECLARE @szM VARCHAR(8)
	DECLARE @szSender VARCHAR(255)

	SET @szTitle = ''
	SET @szMessage = ''

	SELECT szName FROM TCHARTABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT = 0)
		RETURN 1

	SELECT @szTitle = CAST(szTitle AS VARCHAR(255)), @szMessage =  CAST(szMessage AS VARCHAR(1024)), @szSender = CAST(szNPCName AS VARCHAR(255)) FROM TQTITLECHART WHERE dwQuestID = @dwQuestID
	IF(@@ROWCOUNT = 0 )
		RETURN 2

	IF( @szTitle IS NULL OR @szMessage IS NULL OR @szSender IS NULL )
		RETURN 2

	SET @bLenTitle = DATALENGTH(@szTitle)
	SET @bLenMessage = DATALENGTH(@szMessage)
	SET @szT = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenTitle), 8)
	SET @szTitle  = @szT + @szTitle
	SET @szM = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenMessage), 8)
	SET @szMessage = @szM + @szMessage

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
	bGem) VALUES(
	@dwCharID,
	@szSender,
	@szTitle,
	@szMessage,
	0,
	@wItemID,
	@bLevel,
	@bCount,
	@bGLevel,
	@dwDuraMax,
	@dwDuraCur,
	@bRefineCur,
	@dEndTime,
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
	@dwTime6,
	0)

	RETURN 0


