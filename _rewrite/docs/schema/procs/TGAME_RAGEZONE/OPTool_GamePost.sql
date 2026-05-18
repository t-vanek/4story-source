
/*
  *
  *
  */
CREATE       PROCEDURE OPTool_GamePost

	@pIP		VARCHAR(15),
	@pGMID	VARCHAR(10), 

	@pCharID	INT,

	@pSender 	VARCHAR(50),
	@pTitle		VARCHAR(50),
	@pMessage	VARCHAR(2048),

	@pItemID	INT,

	@pLevel	INT,
	@pCount	INT,

	@pGLevel	INT,

	@pRefineCur	INT,

	@dEndTime	SMALLDATETIME,

	@pMagic1	INT,
	@pMagic2	INT,
	@pMagic3	INT,
	@pMagic4	INT,
	@pMagic5	INT,
	@pMagic6	INT,

	@pValue1	INT,
	@pValue2	INT,
	@pValue3	INT,
	@pValue4	INT,
	@pValue5	INT,
	@pValue6	INT,

	@pTime1	INT,
	@pTime2	INT,
	@pTime3	INT,
	@pTime4	INT,
	@pTime5	INT,
	@pTime6	INT	
AS

	DECLARE 	@szRecv	VARCHAR(50)
	DECLARE 	@dwMakeID	 INT
	DECLARE	@dlID 		BIGINT
	
	DECLARE 	@strName	VARCHAR(20)
	DECLARE 	@strItem	VARCHAR(50)
	DECLARE 	@strLog	VARCHAR(1000)


	SELECT @szRecv = szName  FROM TCHARTABLE WHERE dwCharID = @pCharID
	IF @@ROWCOUNT = 0
	BEGIN
		RETURN 0
	END

	DECLARE	@szTitle	VARCHAR(50)
	DECLARE	@szMessage	VARCHAR(500)


	DECLARE	@bLenTitle	BINARY(4)
	DECLARE	@bLenMessage BINARY(4)
	DECLARE 	@szT 		VARCHAR(8)
	DECLARE	@szM 		VARCHAR(8)


	SET @bLenTitle 	= DATALENGTH(@pTitle)
	SET @bLenMessage 	= DATALENGTH(@pMessage)
	SET @szT 		= RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenTitle), 8)

	SET @szTitle  		= @szT + @pTitle

	SET @szM 		= RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenMessage), 8)
	SET @szMessage 	= @szM + @pMessage

	--
	--	INSERT POST
	--
	INSERT INTO TPOSTTABLE
	(
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
		timeRecv
	) 
	VALUES
	(
		@pCharID,
		@pSender,
		0,
		@szRecv,
		@szTitle,
		@szMessage,
		1,
		0,
		0,
		0,
		0,
		GetDate()
	)
	
	SET @dwMakeID = @@IDENTITY

	--
	--	INSERT ATTATCHED ITEM
	--
	IF @pItemID > 0
	BEGIN

		DECLARE @dwDuraMax INT
		SET @dwDuraMax = 0
	
		SELECT @dwDuraMax = dwDuraMax FROM TITEMCHART WHERE wItemID = @pItemID


		IF EXISTS(SELECT TOP 1 dwPostID FROM TPOSTTABLE WHERE dwPostID = @dwMakeID)
		BEGIN

			EXEC  TGenerateDBItemID @dlID  OUTPUT	

			INSERT INTO TITEMTABLE
			(
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
				dwTime6
			) 
			VALUES
			(
				@dlID,
				2,
				@dwMakeID,
				0,
				@pCharID,
				0,
				@pItemID,
				0,
				@pCount,
	
				@pGLevel,
	
				@dwDuraMax,
				@dwDuraMax,
	
				@pRefineCur,

				@dEndTime,
	
				@pMagic1,
				@pMagic2,
				@pMagic3,
				@pMagic4,
				@pMagic5,
				@pMagic6,
				
				@pValue1,
				@pValue2,
				@pValue3,
				@pValue4,
				@pValue5,
				@pValue6,
				
				@pTime1,
				@pTime2,
				@pTime3,
				@pTime4,
				@pTime5,
				@pTime6	
			)
	
		END

	END



	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- Make Log
	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	IF @dwMakeID <> 0
	BEGIN

		--
		-- Get Character Name
		--
		SELECT	 @strName = szName
		FROM		TCHARTABLE
		WHERE	dwCharID  = @pCharID

		SET	@strLog = 		' Sender:' 		+ CONVERT( VARCHAR, @pSender 	)
		SET	@strLog =  @strLog +	' Title:' 			+ CONVERT( VARCHAR, @pTitle 	)
		SET	@strLog =  @strLog +	' Message:' 		+ CONVERT( VARCHAR, @pMessage 	)
		SET	@strLog =  @strLog +	' ' + @strName + ' (' 	+  CONVERT( VARCHAR, @pCharID 	) + ') '

		IF @pItemID > 0
		BEGIN

			--
			-- Get Item Name
			--	
			SELECT	@strItem	= szName	
			FROM		TITEMCHART
			WHERE	wItemID	 	= @pItemID

			SET	@strLog  = @strLog +	@strItem + ' ('	+ CONVERT( VARCHAR, @pItemID	)	+ ')'

			IF @pLevel <> 0
			BEGIN
				SET	@strLog  = @strLog + 	' bLevel:' 	+ CONVERT( VARCHAR, @pLevel	)
			END
	
			SET	@strLog  = @strLog + 	' bCount:' 	+ CONVERT( VARCHAR, @pCount	)
	

			IF @pGLevel <> 0
			BEGIN
				SET	@strLog  = @strLog + 	' bGLevel:' 	+ CONVERT( VARCHAR, @pGLevel	)
			END

		
			IF @pRefineCur <> 0
			BEGIN
				SET	@strLog  = @strLog + 	' bRefineCur:' 	+ CONVERT( VARCHAR, @pRefineCur	)
			END
	
			IF @pMagic1 <> 0
			BEGIN				
				SET	@strLog  = @strLog + 	' OP1('  + CONVERT( VARCHAR, @pMagic1) + ',' + CONVERT( VARCHAR, @pValue1) + ',' +  CONVERT( VARCHAR, @pTime1) + ')'
			END	
	
			IF @pMagic2 <> 0
			BEGIN				
				SET	@strLog  = @strLog + 	' OP2('  + CONVERT( VARCHAR, @pMagic2) + ',' + CONVERT( VARCHAR, @pValue2) + ',' +  CONVERT( VARCHAR, @pTime2) + ')'
			END	
		
			IF @pMagic3 <> 0
			BEGIN				
				SET	@strLog  = @strLog + 	' OP3('  + CONVERT( VARCHAR, @pMagic3) + ',' + CONVERT( VARCHAR, @pValue3) + ',' +  CONVERT( VARCHAR, @pTime3) + ')'
			END	
		
			IF @pMagic4 <> 0
			BEGIN				
				SET	@strLog  = @strLog + 	' OP4('  + CONVERT( VARCHAR, @pMagic4) + ',' + CONVERT( VARCHAR, @pValue4) + ',' +  CONVERT( VARCHAR, @pTime4) + ')'
			END	
		
			IF @pMagic5 <> 0
			BEGIN				
				SET	@strLog  = @strLog + 	' OP5('  + CONVERT( VARCHAR, @pMagic5) + ',' + CONVERT( VARCHAR, @pValue5) + ',' +  CONVERT( VARCHAR, @pTime5) + ')'
			END	
	
			IF @pMagic6 <> 0
			BEGIN				
				SET	@strLog  = @strLog + 	' OP6('  + CONVERT( VARCHAR, @pMagic6) + ',' + CONVERT( VARCHAR, @pValue6) + ',' +  CONVERT( VARCHAR, @pTime6) + ')'
			END	
	
		END
			
		EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'POST', @strLog
		

	END



	RETURN @dwMakeID

