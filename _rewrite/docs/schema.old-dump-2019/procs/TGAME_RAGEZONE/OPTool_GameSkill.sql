
/*
	SKILL
*/
CREATE PROCEDURE OPTool_GameSkill 
	@pIP		VARCHAR(15),
	@pGMID	VARCHAR(10),
	@pDBOP	VARCHAR(10),
	@pCharID	INT,
	@pSkillID	INT,
	@pSkillLevel	INT
AS
	DECLARE @strSkill	VARCHAR(50)
	DECLARE @strName	VARCHAR(20)
	DECLARE @strLog  	VARCHAR(500)

	SELECT	 @strName = szName
	FROM		TCHARTABLE
	WHERE	dwCharID  = @pCharID

	SELECT	@strSkill = szName
	FROM		TSKILLCHART
	WHERE	wID	= 	@pSkillID

	----------------------------------------------------------------------------------------------------------------------------------------
	-- UPDATE	
	----------------------------------------------------------------------------------------------------------------------------------------
	IF @pDBOP = 'U'
	BEGIN

		UPDATE	TSKILLTABLE
		SET		bLevel		=	@pSkillLevel
		WHERE	dwCharID	=	@pCharID
		AND		wSkillID		=	@pSkillID

		SET @strLog = 			@strName +' '	+ @strSkill
		SET @strLog =	@strLog + 	' (' + CONVERT( VARCHAR,  @pCharID 	) +') '
		SET @strLog = 	@strLog +	' wSkillID:' 	+ CONVERT( VARCHAR,  @pSkillID 	)
		SET @strLog = 	@strLog +	' bLevel:' 	+ CONVERT( VARCHAR,  @pSkillLevel 	)

		EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'SKILL UPDATE', @strLog		

	END
	----------------------------------------------------------------------------------------------------------------------------------------
	-- DELETE
	----------------------------------------------------------------------------------------------------------------------------------------
	ELSE IF  @pDBOP = 'D'
	BEGIN

		DELETE	TSKILLTABLE
		WHERE	dwCharID	=	@pCharID
		AND		wSkillID		=	@pSkillID

		SET @strLog = 			@strName +' '	+ @strSkill 
		SET @strLog =	@strLog + 	' (' + CONVERT( VARCHAR,  @pCharID 	) +') '
		SET @strLog = 	@strLog +	' wSkillID:' 	+ CONVERT( VARCHAR, @pSkillID  )

		EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'SKILL DELETE', @strLog		


	END
	----------------------------------------------------------------------------------------------------------------------------------------
	-- INSERT
	----------------------------------------------------------------------------------------------------------------------------------------
	ELSE IF @pDBOP = 'I'
	BEGIN


		INSERT INTO TSKILLTABLE
		(
		dwCharID,
		wSkillID,
		bLevel
		)
		VALUES
		(
		@pCharID,
		@pSkillID,
		1
		)


		SET @strLog = 			@strName +' '	+ @strSkill 
		SET @strLog =	@strLog + 	' (' + CONVERT( VARCHAR,  @pCharID 	) +') '
		SET @strLog = 	@strLog +	' wSkillID:' 	+ CONVERT( VARCHAR, @pSkillID  )

		EXEC   TGLOBAL_GSP.DBO.OPTool_ManagerLog  @pIP, @pGMID, 'SKILL INSERT', @strLog

	END

