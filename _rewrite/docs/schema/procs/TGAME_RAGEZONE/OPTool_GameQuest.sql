
/*
 *	OPTool_GameItem
 *
 *
 *
 */
CREATE      PROCEDURE OPTool_GameQuest
	@pIP		VARCHAR(15),
	@pGMID	VARCHAR(10), 

	@pDBOP	VARCHAR(10),
	@pCharID	INT,

	@pQuestID	INT
AS

	DECLARE @nStack		INT	
	DECLARE @dwDuraMax	INT

	DECLARE @strQuest		VARCHAR(100)
	DECLARE @strName		VARCHAR(20)

	DECLARE @strLog	VARCHAR(500)

	--
	-- Get Character's Name
	--
	SELECT	 @strName = szName
	FROM		TCHARTABLE
	WHERE	dwCharID  = @pCharID

	--
	-- Get Quest's Title
	--
	SELECT 	@strQuest = szTitle
	FROM		TQTITLECHART
	WHERE	dwQuestID = @pQuestID

	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- INSERT 
	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	IF @pDBOP = 'I'
	BEGIN

		INSERT INTO 	TQUESTTABLE		
		(
			dwCharID,
			dwQuestID,
			bTriggerCount,
			bCompleteCount,
			dwTick
		)
		VALUES
		(
			@pCharID,
			@pQuestID,
			1,
			0,
			0
		)

	END
	ELSE IF @pDBOP = 'U'
	BEGIN

		print 'U'

	END
	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- DELETE
	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	ELSE IF @pDBOP = 'D'
	BEGIN
	
		DELETE 	TQUESTTERMTABLE 	WHERE	dwCharID=@pCharID AND	dwQuestID = @pQuestID

		DELETE	TQUESTTABLE		WHERE	dwCharID = @pCharID AND	dwQuestID = @pQuestID

	END
	ELSE IF @pDBOP = 'DA'
	BEGIN
	
		DELETE 	TQUESTTERMTABLE 	WHERE	dwCharID=@pCharID 

		DELETE	TQUESTTABLE		WHERE	dwCharID = @pCharID 

	END

	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	-- Make Log
	------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	IF @@ROWCOUNT <> 0
	BEGIN


		SET	@strLog = @strName +' ' 

		IF @pDBOP = 'I'
		BEGIN	
			
			SET	@strLog =	@strLog + @strQuest + ' (' + CONVERT( VARCHAR, @pQuestID)  +')'
			EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'QUEST CREATE', @strLog

		END
		ELSE IF @pDBOP = 'D'
		BEGIN

			SET	@strLog =	@strLog + @strQuest + ' (' + CONVERT( VARCHAR, @pQuestID)  +')'
			EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'QUEST DELETE', @strLog

		END
		ELSE IF @pDBOP = 'DA'
		BEGIN

			EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'QUEST ALL DELETE', @strLog
		END

	END

