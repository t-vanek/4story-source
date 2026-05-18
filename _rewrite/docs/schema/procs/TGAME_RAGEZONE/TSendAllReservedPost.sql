
CREATE PROCEDURE [dbo].[TSendAllReservedPost]

 AS
	DECLARE	@dwCount	INT

	SELECT @dwCount = count(*) FROM TRESERVEDPOST WHERE bSend = 0
	IF(@dwCount = 0 )
		RETURN 1

	DECLARE SEND_RESERVEDPOST CURSOR
	FOR
	SELECT dwSeq,dwRecverID,szSender,szTitle,szMessage,wItemID,bLevel,bCount,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,dEndTime,bGradeEffect,
	bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6  FROM TRESERVEDPOST WHERE bSend = 0

	OPEN SEND_RESERVEDPOST
	
	DECLARE @dwSeq		INT			
	DECLARE @dwRecverID	INT			
	DECLARE @szSender		VARCHAR(50)		
	DECLARE @szTitle		VARCHAR(256)		
	DECLARE @szMessage		VARCHAR(2048)	
	DECLARE @wItemID		SMALLINT		
	DECLARE @bLevel		TINYINT		
	DECLARE @bCount		TINYINT		
	DECLARE @bGLevel		TINYINT		
	DECLARE @dwDuraMax	INT			
	DECLARE @dwDuraCur		INT			
	DECLARE @bRefineCur		TINYINT		
	DECLARE @dEndTime		SMALLDATETIME	
	DECLARE @bGradeEffect	TINYINT		
	DECLARE @bMagic1		TINYINT		
	DECLARE @bMagic2		TINYINT		
	DECLARE @bMagic3		TINYINT		
	DECLARE @bMagic4		TINYINT		
	DECLARE @bMagic5		TINYINT		
	DECLARE @bMagic6		TINYINT		
	DECLARE @wValue1		SMALLINT		
	DECLARE @wValue2		SMALLINT		
	DECLARE @wValue3		SMALLINT		
	DECLARE @wValue4		SMALLINT		
	DECLARE @wValue5		SMALLINT		
	DECLARE @wValue6		SMALLINT		
	DECLARE @dwTime1		INT			
	DECLARE @dwTime2		INT			
	DECLARE @dwTime3		INT			
	DECLARE @dwTime4		INT			
	DECLARE @dwTime5		INT			
	DECLARE @dwTime6		INT			

	DECLARE @dwPostID		INT			
	DECLARE @dwSenderID	INT			
	DECLARE @szRecver		VARCHAR(256)		
	DECLARE @dlID		BIGINT			
	DECLARE @bItemID		TINYINT		

	FETCH NEXT FROM SEND_RESERVEDPOST INTO @dwSeq,@dwRecverID,@szSender,@szTitle,@szMessage,@wItemID,@bLevel,@bCount,@bGLevel,
								@dwDuraMax,@dwDuraCur,@bRefineCur,@dEndTime,@bGradeEffect,
								@bMagic1,@bMagic2,@bMagic3,@bMagic4,@bMagic5,@bMagic6,
								@wValue1,@wValue2,@wValue3,@wValue4,@wValue5,@wValue6,
								@dwTime1,@dwTime2,@dwTime3,@dwTime4,@dwTime5,@dwTime6
	WHILE @@FETCH_STATUS = 0
	BEGIN
		EXEC TGetReservedPost @dwPostID,@dwSenderID,@dwRecverID,@szSender,@szRecver,@szTitle,@szMessage,@dlID,@bItemID,@wItemID,@bLevel,@bCount,@bGLevel,
						@dwDuraMax,@dwDuraCur,@bRefineCur,@dEndTime,@bGradeEffect,
						@bMagic1,@bMagic2,@bMagic3,@bMagic4,@bMagic5,@bMagic6,
						@wValue1,@wValue2,@wValue3,@wValue4,@wValue5,@wValue6,
						@dwTime1,@dwTime2,@dwTime3,@dwTime4,@dwTime5,@dwTime6

	FETCH NEXT FROM SEND_RESERVEDPOST INTO @dwSeq,@dwRecverID,@szSender,@szTitle,@szMessage,@wItemID,@bLevel,@bCount,@bGLevel,
								@dwDuraMax,@dwDuraCur,@bRefineCur,@dEndTime,@bGradeEffect,
								@bMagic1,@bMagic2,@bMagic3,@bMagic4,@bMagic5,@bMagic6,
								@wValue1,@wValue2,@wValue3,@wValue4,@wValue5,@wValue6,
								@dwTime1,@dwTime2,@dwTime3,@dwTime4,@dwTime5,@dwTime6
	END
	CLOSE SEND_RESERVEDPOST
	DEALLOCATE SEND_RESERVEDPOST
	
	RETURN 0


