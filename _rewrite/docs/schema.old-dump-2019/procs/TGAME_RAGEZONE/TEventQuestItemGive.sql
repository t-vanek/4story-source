
CREATE PROCEDURE [dbo].[TEventQuestItemGive] 
@dwCharID INT,
@dwID INT,
@bCount TINYINT,
@szTitle VARCHAR(256),
@szMessage VARCHAR(1024)
AS

DECLARE	@bLenTitle		BINARY(4)
DECLARE	@bLenMessage 	BINARY(4)
DECLARE 	@szT 			VARCHAR(8)
DECLARE 	@szM 			VARCHAR(8)
DECLARE	@dEndTime		SMALLDATETIME
DECLARE 	@dCur 			SMALLDATETIME
DECLARE 	@wUseTime		SMALLINT
DECLARE	@szSender		VARCHAR(50)

SELECT @szSender = szMessage FROM TSVRMSGCHART WHERE dwID = 6

SET @dEndTime = 0
SET @bLenTitle = DATALENGTH(@szTitle)
SET @bLenMessage = DATALENGTH(@szMessage)
SET @szT = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenTitle), 8)
SET @szTitle  = @szT + @szTitle
SET @szM = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenMessage), 8)
SET @szMessage = @szM + @szMessage

SELECT @wUseTime = wUseTime  FROM TQUESTITEMCHART WHERE dwID = @dwID
IF( @wUseTime <> 0 )
BEGIN
	SET @dCur = DATEADD(dd,-1,DATEADD(mm,1,GETDATE()-DAY(GETDATE())+1))
	SET @dEndTime = 0
	SET @dEndTime=dateadd(year,datepart(year,@dCur)-1900, @dEndTime)
	SET @dEndTime=dateadd(month,datepart(month,@dCur)-1, @dEndTime)
	SET @dEndTime=dateadd(day,datepart(day,@dCur-1), @dEndTime)
	SET @dEndTime=dateadd(second,86340, @dEndTime)
END

INSERT INTO TRESERVEDPOST(
	dwRecverID,szSender,szTitle,szMessage,bSend,
	wItemID,bLevel,bCount,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,dEndTime,
	bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,
	wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,
	dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6,bGem,wMoggItemID) 
SELECT @dwCharID,@szSender,@szTitle,@szMessage,0,
	wItemID,bLevel,@bCount,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,@dEndTime,
	bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,
	wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,
	dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem, 0
FROM TQUESTITEMCHART WHERE dwID = @dwID


