

CREATE PROCEDURE [dbo].[TCreateChar_Dummy]
	@bCreateCnt		TINYINT	OUTPUT,
	@szNAME		VARCHAR(50),
	@dwCharID		INT		OUTPUT,
	@dwUserID		INT,
	@bGroup		TINYINT,
	@bSlot			TINYINT,
	@bClass		TINYINT,
	@bRace		TINYINT,
	@bCountry		TINYINT,
	@bSex			TINYINT,
	@bHair			TINYINT,
	@bFace		TINYINT,
	@bBody		TINYINT,
	@bPants		TINYINT,
	@bHand		TINYINT,
	@bFoot			TINYINT
AS
	DECLARE @dwCharSeq INT
	DECLARE @dwMP	INT
	DECLARE @dwHP	INT
	DECLARE @fPosX	FLOAT
	DECLARE @fPosY	FLOAT
	DECLARE @fPosZ	FLOAT
	DECLARE @wDIR	SMALLINT
	DECLARE @wSpawnID SMALLINT
	DECLARE @nCheckGlobal INT
	DECLARE @bRealSex	TINYINT
	
	DECLARE @dwCenterX		INT
	DECLARE @dwCenterZ 		INT
	DECLARE @dwRandomX	INT
	DECLARe @dwRandomZ 	INT

	SET @bCreateCnt = 0
	SET @dwCharID = 0
	SET @dwMP = 2
	SET @dwHP = 2

	SELECT TOP 1 @dwHP = @dwHP + wCON, @dwMP = @dwMP + wMEN FROM TCLASSCHART WHERE bClassID = @bClass
	SELECT TOP 1 @dwHP = @dwHP + wCON, @dwMP = @dwMP + wMEN FROM TRACECHART WHERE bRaceID = @bRace

	SET @dwHP = 7 * @dwHP + 1
	SET @dwMP = 9 * @dwMP + 1

	IF EXISTS( SELECT TOP 1 dwCharID FROM TCHARTABLE WHERE szNAME = @szNAME)
		RETURN 2
	/*
   	 * Duplicate npc name 
	 */
	IF EXISTS(SELECT TOP 1 * FROM TNPCCHART WHERE szNAME = @szNAME)
		RETURN 2
	/*
	 * Duplicate monster name
	 */
	IF EXISTS(SELECT TOP 1 * FROM TMONSTERCHART WHERE szNAME = @szNAME)
		RETURN 2
	/*
	 * Invalid slot
	 */
	IF EXISTS(SELECT TOP 1 dwCharID FROM TCHARTABLE WHERE dwUserID = @dwUserID AND bSlot = @bSlot AND bDelete = 0)
		RETURN 3
	/*
 	 * Check the country
	 */
	IF NOT EXISTS(SELECT TOP 1 bCountry FROM  TCHARTABLE WHERE dwUserID = @dwUserID AND bCountry = @bCountry AND bDelete = 0) 
	BEGIN
		IF EXISTS(SELECT TOP 1 dwUserID FROM TCHARTABLE WHERE dwUserID = @dwUserID AND bDelete = 0)
			RETURN 4
	END


	SET @fPosX 	=  RAND() * 1024 + 1024 * 2   --5375.51
	SET @fPosY 	= 0
	SET @fPosZ 	=  RAND() * 1024 + 1024 * 4
	SET @wDIR 	= 165
	SET @wSpawnID = 13
	

	EXEC @nCheckGlobal = TGLOBAL_GSP.DBO.TCreateChar
		@dwCharSeq  OUTPUT,
		@bCreateCnt OUTPUT,
		@bRealSex OUTPUT,
		@dwUserID,
		@bGroup,
		@dwCharID,
		@bSlot,
		@szNAME,
		@bClass,
		@bRace,
		@bCountry,
		@bSex,
		@bHair,		-- bHair
		@bFace,	-- bFace
		@bBody,	-- bBody
		@bPants,	-- bPants
		@bHand,	-- bHand
		@bFoot,	-- bFoot		
		1, 0

	IF(@nCheckGlobal <> 0)
		RETURN @nCheckGlobal

	BEGIN TRAN TCREATECHAR

	INSERT INTO TCHARTABLE
	(
		dwUserID,
		bSlot,
		szNAME,
		bRace,
		bCountry,
		bRealSex,
		bSex,
		bClass,
		bLevel,
		bHair,
		bFace,
		bBody,
		bPants,
		bHand,
		bFoot,
		dwEXP,
		dwHP,
		dwMP,
		dwGold,
		dwSilver,
		dwCooper,
		wMapID,
		wSpawnID,
		wTemptedMon,
		bAftermath,
		fPosX,
		fPosY,
		fPosZ,
		wDIR
	) 
	VALUES
	(
		@dwUserID,	-- dwUserID
		@bSlot,		-- bSlot
		@szNAME,	-- szNAME
		@bRace,	-- bRace
		@bCountry,	-- bCountry
		@bRealSex,	--bRealSex
		@bSex,		-- bSex
		@bClass,	-- bClass
		80,		-- bLevel
		@bHair,		-- bHair
		@bFace,	-- bFace
		@bBody,	-- bBody
		@bPants,	-- bPants
		@bHand,	-- bHand
		@bFoot,	-- bFoot
		0,		-- dwEXP
		@dwHP,	-- dwHP

		@dwMP,	-- dwMP
		100,		-- dwGold
		0,		-- dwSilver
		0,		-- dwCooper
		0,		-- wMapID
		@wSpawnID,	-- wSpawnID
		0,		-- wTemptedMon
		0,		-- bAftermath
		@fPosX,	-- wPosX
		@fPosY,	-- wPosY
		@fPosZ,	-- wPosZ
		@wDIR		-- wDIR
	)

	SET @dwCharID = @@IDENTITY
	INSERT INTO TINVENTABLE
	(
		dwCharID,
		bInvenID,
		wItemID,
		dEndTime
	) 
	VALUES
	(
		@dwCharID,
		255, 
		3,
		0
	)

	INSERT INTO TINVENTABLE
	(
		dwCharID,
		bInvenID,
		wItemID,
		dEndTime
	) 
	VALUES
	(
		@dwCharID,
		254, 
		2,
		0
	)

	INSERT INTO TCABINETTABLE VALUES(@dwCharID, 0, 1)
--	INSERT INTO TSKILLTABLE SELECT @dwCharID, wSkillID, 1, 0 FROM TSTARTSKILL WHERE @bClass = bClassID
	INSERT INTO TSKILLTABLE SELECT @dwCharID, wID, 1, 0 FROM TSKILLCHART WHERE (dwClassid & power(2,@bClass) <> 0) and bCanLearn =1 
	COMMIT TRAN TCREATECHAR

	UPDATE TGLOBAL_GSP.DBO.TALLCHARTABLE SET dwCharID = @dwCharID WHERE dwSeq = @dwCharSeq
	INSERT INTO THOTKEYTABLE 
		SELECT @dwCharID, bInvenID, bType1, wID1, bType2, wID2, bType3, wID3, bType4, wID4, bType5, wID5,
			  bType6, wID6, bType7, wID7, bType8, wID8, bType9, wID9, bType10, wID10, bType11, wID11, bType12, wID12 
		FROM TSTARTHOTKEY WHERE bClassID=@bClass

	IF EXISTS( SELECT bClassID FROM TSTARTRECALL WHERE bClassID = @bClass AND bCountryID = @bCountry)
	BEGIN
		DECLARE @dwMonID INT
		DECLARE @wMonTemp SMALLINT
		DECLARE @dwATTR INT
		DECLARE @dwMaxHP INT
		DECLARE @dwMaxMP INT
		SET @wMonTemp = 0
		SELECT @wMonTemp = wMonID FROM TSTARTRECALL WHERE bClassID = @bClass AND bCountryID = @bCountry
		IF(@wMonTemp > 0)
		BEGIN
			SELECT @dwATTR = wSummonAttr FROM TMONSTERCHART WHERE wID= @wMonTemp
			SELECT @dwMaxHP=dwMaxHP, @dwMaxMP = dwMaxMP FROM TMONATTRCHART WHERE wID=@dwATTR AND bLevel=1
			UPDATE TCHARTABLE SET wTemptedMon = @wMonTemp WHERE dwCharID = @dwCharID
			SET @dwATTR = @dwATTR + POWER(2,16)
			EXEC TCreateRecallMon 
				@dwMonID OUTPUT, 
				@dwCharID, 
				@wMonTemp, 
				0, 
				@dwATTR, 
				1, 
				@dwMaxHP, 
				@dwMaxMP, 
				1,
				@fPosX, 
				@fPosY,
				@fPosZ,
				0
		END
	END


	RETURN @@ERROR


