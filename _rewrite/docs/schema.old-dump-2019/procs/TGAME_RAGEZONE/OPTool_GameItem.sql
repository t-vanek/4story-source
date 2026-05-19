
/*
 *	OPTool_GameItem
 *
 *
 *
 */
CREATE        PROCEDURE OPTool_GameItem
	@pIP		VARCHAR(15),
	@pGMID	VARCHAR(10), 

	@pDBOP	VARCHAR(10),
	@pCharID	INT,
	@pInven	INT,
	@pSlot		INT,
	@pItemID	INT,
	@pLevel	INT,
	@pCount	INT,

	@pGLevel	INT,
	@pDuraMax	INT,
	@pDuraCur	INT,
	@pRefineCur	INT,

	@dEndTime	SMALLDATETIME, 
	
	@pGradeEffect	INT,

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

	DECLARE @nStack		INT	
	DECLARE @dwDuraMax	INT
	DECLARE @dlID 		BIGINT

	DECLARE @strName		VARCHAR(20)
	DECLARE @strItem		VARCHAR(50)

	DECLARE @strLog		VARCHAR(500)

	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- INSERT 
	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	IF @pDBOP = 'I'
	BEGIN

		EXEC  TGenerateDBItemID @dlID  OUTPUT


		SELECT @nStack = bStack,
			@dwDuraMax = dwDuraMax
		FROM	TITEMCHART	WHERE	wItemID = @pItemID
		
		INSERT INTO TITEMTABLE
		(
			dlID,
			dwOwnerID,
			bOwnerType,

			bStorageType,
			dwStorageID,

			bItemID,
			wItemID,
			bLevel,
			bCount,
			bGLevel,
	
			dwDuraMax,
			dwDuraCur,
			bRefineCur,

			bGradeEffect,

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

			dEndTime
		)
		VALUES
		(
			@dlID,
			@pCharID,
			0,

			0,
			@pInven,

			@pSlot,
			@pItemID,
			0,
			@nStack,
			0,
	
			@dwDuraMax,
			@dwDuraMax,
			0,

			@pGradeEffect,

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
			@pTime6,

			@dEndTime
	
		)

		
		

	END
	ELSE IF @pDBOP = 'U'
	BEGIN

		SELECT @pItemID = wItemID
		FROM	 TITEMTABLE
		WHERE dwOwnerID 	= @pCharID
		AND	 dwStorageID  	= @pInven
		AND	 bItemID    	= @pSlot


		UPDATE TITEMTABLE
		SET	bLevel	 	= @pLevel,
			bCount	 	= @pCount,
			bGLevel	= @pGLevel,
			dwDuraMax	= @pDuraMax,
			dwDuraCur	= @pDuraCur,
			bRefineCur	= @pRefineCur,
			dEndTime	= @dEndTime,
			bGradeEffect	= @pGradeEffect,
			bMagic1	= @pMagic1,
			bMagic2	= @pMagic2,
			bMagic3 	= @pMagic3,
			bMagic4 	= @pMagic4,
			bMagic5 	= @pMagic5,
			bMagic6 	= @pMagic6,
			wValue1 	= @pValue1,
			wValue2 	= @pValue2,
			wValue3 	= @pValue3,
			wValue4 	= @pValue4,
			wValue5 	= @pValue5,
			wValue6 	= @pValue6,
			dwTime1 	= @pTime1,
			dwTime2 	= @pTime2,
			dwTime3 	= @pTime3,
			dwTime4 	= @pTime4,
			dwTime5 	= @pTime5,
			dwTime6 	= @pTime6
		WHERE dwOwnerID 	= @pCharID
		AND	 dwStorageID  	= @pInven
		AND	 bItemID    	= @pSlot

		


	END
	ELSE IF @pDBOP = 'D'
	BEGIN


		SELECT @pItemID = wItemID
		FROM	 TITEMTABLE
		WHERE dwOwnerID 	= @pCharID
		AND	 dwStorageID  	= @pInven
		AND	 bItemID    	= @pSlot
	
		SELECT @pLevel		= bLevel,
			@pCount		= bCount,
			@pGradeEffect	= bGradeEffect,
			@pMagic1	= bMagic1,
			@pMagic2	= bMagic2,
			@pMagic3	= bMagic3,
			@pMagic4	= bMagic4,
			@pMagic5	= bMagic5,
			@pMagic6	= bMagic6,

			@pValue1	= wValue1,
			@pValue2	= wValue2,
			@pValue3	= wValue3,
			@pValue4	= wValue4,
			@pValue5	= wValue5,
			@pValue6	= wValue6 ,

			@pTime1	= dwTime1,
			@pTime2	= dwTime2,
			@pTime3	= dwTime3,
			@pTime4	= dwTime4,
			@pTime5	= dwTime5 ,
			@pTime6	= dwTime6
		FROM	TITEMTABLE
		WHERE dwOwnerID = @pCharID
		AND	 dwStorageID  = @pInven



		DELETE TITEMTABLE				
		WHERE dwOwnerID = @pCharID
		AND	 dwStorageID  = @pInven
		AND	 bItemID    =  @pSlot

	END


	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- Make Log
	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	IF @@ROWCOUNT <> 0
	BEGIN

		--
		-- Get Character Name
		--
		SELECT	 @strName = szName
		FROM		TCHARTABLE
		WHERE	dwCharID  = @pCharID
	
		--
		-- Get Item Name
		--	
		SELECT	@strItem	= szName	
		FROM		TITEMCHART
		WHERE	wItemID	 	= @pItemID


		SET	@strLog  =		@strName + ' (' 	+ CONVERT( VARCHAR, @pCharID ) + ') '
		SET	@strLog  = @strLog + 	@strItem
		SET	@strLog  = @strLog + 	' bInvenID:' 	+ CONVERT( VARCHAR, @pInven	)
		SET	@strLog  = @strLog + 	' bItemID:' 	+ CONVERT( VARCHAR, @pSlot	)
		SET	@strLog  = @strLog +	' wItemID:'	+ CONVERT( VARCHAR, @pItemID	)

		IF @pLevel <> 0
		BEGIN
			SET	@strLog  = @strLog + 	' bLevel:' 	+ CONVERT( VARCHAR, @pLevel	)
		END

		SET	@strLog  = @strLog + 	' bCount:' 	+ CONVERT( VARCHAR, @pCount	)


		IF @pGLevel <> 0
		BEGIN
			SET	@strLog  = @strLog + 	' bGLevel:' 	+ CONVERT( VARCHAR, @pGLevel	)
		END

		IF @pDuraMax <> 0
		BEGIN
			SET	@strLog  = @strLog + 	' dwDuraMax:' 	+ CONVERT( VARCHAR, @pDuraMax	)
		END

		IF @pDuraCur <> 0
		BEGIN
			SET	@strLog  = @strLog + 	' dwDuraCur:' 	+ CONVERT( VARCHAR, @pDuraCur	)
		END

		
		IF @pRefineCur <> 0
		BEGIN
			SET	@strLog  = @strLog + 	' bRefineCur:' 	+ CONVERT( VARCHAR, @pRefineCur	)
		END

		IF @pGradeEffect <> 0
		BEGIN
			SET	@strLog  = @strLog + 	'bGradeEffect:' 	+ CONVERT( VARCHAR, @pGradeEffect	)
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



		IF @pDBOP = 'I'
		BEGIN					
			EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'ITEM CREATE', @strLog
		END
		ELSE IF @pDBOP = 'U'
		BEGIN
			EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'ITEM UPDATE', @strLog
		END
		ELSE IF @pDBOP = 'D'
		BEGIN
			EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'ITEM DELETE', @strLog
		END
		

	END

