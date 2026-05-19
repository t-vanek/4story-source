


CREATE PROCEDURE [dbo].[TGetReservedPost] 
	@dwPostID	INT 		OUTPUT,
	@dwSenderID 	INT 		OUTPUT,
	@dwRecverID	INT		OUTPUT,
	@szSender 	VARCHAR(50) 	OUTPUT,
	@szRecver	VARCHAR(50)	OUTPUT,
	@szTitle	VARCHAR(256) 	OUTPUT,
	@szMessage	VARCHAR(2048) OUTPUT,
	@dlID		BIGINT 		OUTPUT,
	@bItemID	TINYINT 	OUTPUT,
	@wItemID	SMALLINT 	OUTPUT,
	@bLevel	TINYINT 	OUTPUT,
	@bCount	TINYINT 	OUTPUT,
	@bGLevel	TINYINT 	OUTPUT,
	@dwDuraMax	INT		OUTPUT,
	@dwDuraCUr	INT		OUTPUT,
	@bRefineCur	TINYINT	OUTPUT,
	@dEndTime	SMALLDATETIME OUTPUT,
	@bGradeEffect	TINYINT	OUTPUT,
	@bMagic1	TINYINT	OUTPUT,
	@bMagic2	TINYINT	OUTPUT,
	@bMagic3	TINYINT	OUTPUT,
	@bMagic4	TINYINT	OUTPUT,
	@bMagic5	TINYINT	OUTPUT,
	@bMagic6	TINYINT	OUTPUT,
	@wValue1	SMALLINT	OUTPUT,
	@wValue2	SMALLINT	OUTPUT,
	@wValue3	SMALLINT	OUTPUT,
	@wValue4	SMALLINT	OUTPUT,
	@wValue5	SMALLINT	OUTPUT,
	@wValue6	SMALLINT	OUTPUT,
	@dwTime1	INT		OUTPUT,
	@dwTime2	INT		OUTPUT,
	@dwTime3	INT		OUTPUT,
	@dwTime4	INT		OUTPUT,
	@dwTime5	INT		OUTPUT,
	@dwTime6	INT		OUTPUT,
	@bGem  		TINYINT OUTPUT,
	@wMoggItemID smallint OUTPUT

 AS

DECLARE @Ret	INT
DECLARE @dwSeq 	INT
DECLARE @CurTime	SMALLDATETIME
DECLARE @DBID	BIGINT
DECLARE @dwCount 	INT
DECLARE @TdwPostID INT
DECLARE @TdwRecverID INT

SET @bItemID = 0

SELECT @dwCount = COUNT(dwSeq) FROM TRESERVEDPOST WHERE bSend = 0

IF(@dwCount = 0)
   RETURN 1


SELECT TOP 1
	@dwSeq = dwSeq, 	
	@dwRecverID = dwRecverID, 
	@szSender = szSender, 	
	@szTitle = szTitle, 
	@szMessage = szMessage, 
	@wItemID = wItemID, 
	@bLevel = bLevel, 
	@bCount = bCount, 
	@bGLevel = bGLevel, 
	@dwDuraMax = dwDuraMax, 
	@dwDuraCur = dwDuraCur, 
	@bRefineCur = bRefineCur, 
	@dEndTime = dEndTime,
	@bGradeEffect = bGradeEffect,
	@bMagic1 = bMagic1, 
	@bMagic2 = bMagic2, 
	@bMagic3 = bMagic3, 
	@bMagic4 = bMagic4, 
	@bMagic5 = bMagic5, 
	@bMagic6 = bMagic6, 
	@wValue1 = wValue1, 
	@wValue2 = wValue2, 
	@wValue3 = wValue3, 
	@wValue4 = wValue4, 
	@wValue5 = wValue5, 
	@wValue6 = wValue6, 
	@dwTime1 = dwTime1, 
	@dwTime2 = dwTime2, 
	@dwTime3 = dwTime3, 
	@dwTime4 = dwTime4, 
	@dwTime5 = dwTime5, 
	@dwTime6 = dwTime6,
	@bGem		= bGem,
	@wMoggItemID = wMoggItemID

	FROM TRESERVEDPOST WHERE bSend = 0
	
	SELECT @szRecver = szName FROM TCHARTABLE WHERE dwCharID = @dwRecverID
	IF(@@ROWCOUNT = 0 OR @szRecver IS NULL )
	BEGIN
		UPDATE TRESERVEDPOST SET bSend = 2 WHERE dwSeq = @dwSeq
		RETURN 2
	END
	
	SET @dwSenderID = 0
	SELECT @dwSenderID = dwCharID FROM TCHARTABLE WHERE szName = @szSender	

	SELECT @CurTime = (SELECT CONVERT( SMALLDATETIME , GETDATE() ) )
	
	EXEC @Ret = TSavePost @TdwPostID OUTPUT, @TdwRecverID OUTPUT, @dwSenderID, @dwRecverID, @szRecver, @szSender, @szTitle, @szMessage, 0,1,0,0,0,  @CurTime

	-- 캐릭터가 없는 경우
	IF ( @Ret > 0 )
	BEGIN
		UPDATE TRESERVEDPOST SET bSend = 2 WHERE dwSeq = @dwSeq
		RETURN 2
	END

	SET @dwPostID = @TdwPostID

	EXEC TGenerateDBItemID @DBID OUTPUT
	SET @dlID = @DBID

	EXEC @Ret = TSaveItemDirect @dlID, 2 , @dwPostID, 0, @dwRecverID, 
			@bItemID,@wItemID,@bLevel,@bCount,@bGLevel,
			@dwDuraMax,@dwDuraCur,@bRefineCur,@dEndTime,@bGradeEffect,
			@bMagic1,@bMagic2,@bMagic3,@bMagic4,@bMagic5,@bMagic6,
			@wValue1,@wValue2,@wValue3,@wValue4,@wValue5,@wValue6,
			@dwTime1,@dwTime2,@dwTime3,@dwTime4,@dwTime5,@dwTime6,@bGem,@wMoggItemID

	UPDATE TRESERVEDPOST SET bSend = 1 WHERE dwSeq = @dwSeq
	
	RETURN 0

