



CREATE PROCEDURE [dbo].[TSaveMonthRank]
	@bMonth		TINYINT,
	@bRank		TINYINT,
	@bMonthRank		TINYINT,
	@dwCharID		INT,
	@szName		VARCHAR(50),
	@dwTotalPoint		INT,
	@dwMonthPoint		INT,
	@wMonthWin		SMALLINT,
	@wMonthLose		SMALLINT,
	@dwTotalWin		INT,
	@dwTotalLose		INT,
	@bCountry		TINYINT,
	@bLevel		TINYINT,
	@bClass		TINYINT,
	@bRace		TINYINT,
	@bSex			TINYINT,
	@bHair			TINYINT,
	@bFace		TINYINT,
	@szSay			VARCHAR(256),
	@szGuild		VARCHAR(50)
 AS

	DECLARE	@bChartType1		TINYINT
	DECLARE	@wItemID1		SMALLINT
	DECLARE	@bCount1		TINYINT
	DECLARE	@bChartType2		TINYINT
	DECLARE	@wItemID2		SMALLINT
	DECLARE	@bCount2		TINYINT
	DECLARE	@bChartType3		TINYINT
	DECLARE	@wItemID3		SMALLINT
	DECLARE	@bCount3		TINYINT
	DECLARE	@bChartType4		TINYINT
	DECLARE	@wItemID4		SMALLINT
	DECLARE	@bCount4		TINYINT
	DECLARE	@szTitle		VARCHAR(256)
	DECLARE	@szMessage		VARCHAR(1024)
	DECLARE	@szTitle2		VARCHAR(256)
	DECLARE	@szMessage2		VARCHAR(1024)
	DECLARE	@szSender		VARCHAR(50)
	DECLARE 	@wUseTime		SMALLINT
	DECLARE	@dEndTime		SMALLDATETIME
	DECLARE 	@dCur 			SMALLDATETIME
	DECLARE	@bLenTitle		BINARY(4)
	DECLARE	@bLenMessage 	BINARY(4)
	DECLARE 	@szT 			VARCHAR(8)
	DECLARE 	@szM 			VARCHAR(8)
	DECLARe	@bSendPost		TINYINT

	IF NOT EXISTS( SELECT dwTotalPoint FROM TPVPOINTTABLE WHERE dwCharID = @dwCharID)
		INSERT INTO TPVPOINTTABLE(dwCharID,dwUseablePoint,dwTotalPoint) VALUES(@dwCharID,@dwTotalPoint,@dwTotalPoint)
	ELSE
		UPDATE TPVPOINTTABLE SET dwTotalPoint = @dwTotalPoint WHERE dwCharID = @dwCharID
	IF NOT EXISTS( SELECT dwPoint FROM TMONTHPVPOINTTABLE WHERE dwCharID = @dwCharID)
		INSERT INTO TMONTHPVPOINTTABLE(dwCharID,bCountry,dwPoint,wWin,wLose,szSay) VALUES(@dwCharID,@bCountry,@dwMonthPoint,@wMonthWin,@wMonthLose,@szSay)
	ELSE
		UPDATE TMONTHPVPOINTTABLE SET dwPoint = @dwMonthPoint WHERE dwCharID = @dwCharID

	SELECT TOP 1 dwCharID FROM TMONTHRANKTABLE WHERE bMonthRank = @bMonthRank
	IF(@@ROWCOUNT = 0 )
		SET @bSendPost = 0
	ELSE 
		SET @bSendPost = 1

	INSERT INTO TMONTHRANKTABLE (
		bMonth,	bCountry,bRank,bMonthRank,dwTotalRank,dwCharID,szName,
		dwTotalPoint,dwMonthPoint,wMonthWin,wMonthLose,dwTotalWin,dwTotalLose,bLevel,bClass,bRace,bSex,bHair,bFace,szSay,szGuild) 
		 VALUES(
		@bMonth,@bCountry,@bRank,@bMonthRank,0,@dwCharID,@szName,
		@dwTotalPoint,@dwMonthPoint,@wMonthWin,@wMonthLose,@dwTotalWin,@dwTotalLose,@bLevel,@bClass,@bRace,@bSex,@bHair,@bFace,@szSay,@szGuild )

	IF(@bMonthRank > 8 OR @bSendPost = 1 OR @bMonthRank = 0  )
		RETURN 0

	
	SELECT @szTitle = szMessage FROM TSVRMSGCHART WHERE dwID = 36
	SELECT @szMessage = szMessage FROM TSVRMSGCHART WHERE dwID = 37
	SELECT @szSender = szMessage FROM TSVRMSGCHART WHERE dwID = 6

	SET @bLenTitle = DATALENGTH(@szTitle)
	SET @bLenMessage = DATALENGTH(@szMessage)
	SET @szT = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenTitle), 8)
	SET @szTitle2  = @szT + @szTitle
	SET @szM = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenMessage), 8)
	SET @szMessage2 = @szM + @szMessage

	SELECT 
		@bChartType1 = bChartType1, @wItemID1 = wItemID1, @bCount1 = bCount1, 
		@bChartType2 = bChartType2, @wItemID2 = wItemID2, @bCount2 = bCount2,  
		@bChartType3 = bChartType3 ,@wItemID3 = wItemID3, @bCount3 = bCount3 , 
		@bChartType4 = bChartType4 ,@wItemID4 = wItemID4, @bCount4 = bCount4 
	FROM TMONTHRANKCHART WHERE bRank = @bMonthRank

	IF(@@ROWCOUNT = 0 )
		RETURN 0

	IF(@bChartType1 = 1)  
	BEGIN
		EXEC TEventItemGive @szName,@wItemID1,@bCount1,@szTitle,@szMessage
	END
	ELSE IF(@bChartType1 = 0  )
	BEGIN	
		SET @dEndTime = 0
		SELECT @wUseTime = wUseTime  FROM TQUESTITEMCHART WHERE dwID = @wItemID1
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
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem) 
		SELECT @dwCharID,@szSender,@szTitle2,@szMessage2,0,
			wItemID,bLevel,@bCount1,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,@dEndTime,
			bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,
			wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem
		FROM TQUESTITEMCHART WHERE dwID = @wItemID1
	
	END
	
	IF(@bChartType2 = 1 AND @wItemID2 <> 0)  
	BEGIN
		EXEC TEventItemGive @szName,@wItemID2,@bCount2,@szTitle,@szMessage
	END
	ELSE IF( @bChartType2 = 0 AND @wItemID2 <> 0)
	BEGIN	
		SET @dEndTime = 0
		SELECT @wUseTime = wUseTime  FROM TQUESTITEMCHART WHERE dwID = @wItemID1
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
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem) 
		SELECT @dwCharID,@szSender,@szTitle2,@szMessage2,0,
			wItemID,bLevel,@bCount2,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,@dEndTime,
			bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,
			wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem
		FROM TQUESTITEMCHART WHERE dwID = @wItemID2	
	END

	
	IF(@bChartType3 = 1 AND @wItemID3 <> 0)  
	BEGIN
		EXEC TEventItemGive @szName,@wItemID3,@bCount3,@szTitle,@szMessage
	END
	ELSE IF( @bChartType3 = 0 AND @wItemID3 <> 0)
	BEGIN	
		SET @dEndTime = 0
		SELECT @wUseTime = wUseTime  FROM TQUESTITEMCHART WHERE dwID = @wItemID1
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
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem) 
		SELECT @dwCharID,@szSender,@szTitle2,@szMessage2,0,
			wItemID,bLevel,@bCount3,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,@dEndTime,
			bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,
			wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem
		FROM TQUESTITEMCHART WHERE dwID = @wItemID3	
	END


	IF(@bChartType4 = 1 AND @wItemID4 <> 0)  
	BEGIN
		EXEC TEventItemGive @szName,@wItemID4,@bCount4,@szTitle,@szMessage
	END
	ELSE IF( @bChartType4 = 0 AND @wItemID4 <> 0)
	BEGIN		
		SET @dEndTime = 0
		SELECT @wUseTime = wUseTime  FROM TQUESTITEMCHART WHERE dwID = @wItemID1
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
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem) 
		SELECT @dwCharID,@szSender,@szTitle2,@szMessage2,0,
			wItemID,bLevel,@bCount4,bGLevel,dwDuraMax,dwDuraCur,bRefineCur,@dEndTime,
			bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,
			wValue1,wValue2,wValue3,wValue4,wValue5,wValue6,
			dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6, bGem
		FROM TQUESTITEMCHART WHERE dwID = @wItemID4
	END


	RETURN 0




