CREATE PROCEDURE [dbo].[TCreateChar]
	@dwCharSeq		INT	  OUTPUT,
	@bCreateCnt		TINYINT OUTPUT,
	@bRealSex		TINYINT OUTPUT,
	@dwUserID		INT,
	@bWorld		TINYINT,
	@dwCharID		INT,
	@bSlot			TINYINT,
	@szNAME		VARCHAR(50),
	@bClass		TINYINT,
	@bRace		TINYINT,
	@bCountry		TINYINT,
	@bSex			TINYINT,
	@bHair			TINYINT,
	@bFace		TINYINT,
	@bBody		TINYINT,
	@bPants		TINYINT,
	@bHand		TINYINT,
	@bFoot			TINYINT,
	@bLevel		TINYINT,
	@dwEXP		INT
AS
	DECLARE @dwReservedID INT

	SET @dwReservedID = 0
	SET @bCreateCnt = 6
	SET @dwCharSeq  = 0
	SET @bRealSex = 0

	IF EXISTS( SELECT TOP 1 dwCharID FROM TALLCHARTABLE WHERE szNAME = @szNAME)
		RETURN 2

	IF EXISTS(SELECT TOP 1 * FROM TKEEPINGNAME WHERE @szName like(szName))
		RETURN 2

--	SELECT TOP 1 @bCreateCnt = bCanCreateCharCount FROM TUSERINFOTABLE WHERE dwUserID = @dwUserID
--	IF( @bCreateCnt < 1 AND EXISTS(SELECT TOP 1  dwUserID FROM TALLCHARTABLE_PW WHERE dwUserID = @dwUserID AND bWorldID = @bWorld AND bDelete = 0))
--		RETURN 6

	SELECT @dwReservedID = dwUserID FROM TRESERVEDNAME WHERE szNAME = @szNAME
	IF(@@ROWCOUNT <> 0 AND @dwReservedID <> @dwUserID)
		RETURN 2

	BEGIN TRAN ALLCHARTABLE

	INSERT INTO TALLCHARTABLE(
		dwUserID,
		bWorldID,
		dwCharID,
		bSlot,
		szNAME,
		bClass,
		bRace,
		bCountry,
		bSex,
		bHair,
		bFace,
		bBody,
		bPants,
		bHand,
		bFoot,
		bLevel,
		dwEXP) VALUES(
		@dwUserID,
		@bWorld,
		@dwCharID,
		@bSlot,
		@szNAME,
		@bClass,
		@bRace,
		@bCountry,
		@bSex,
		@bHair,
		@bFace,
		@bBody,
		@bPants,
		@bHand,
		@bFoot,
		@bLevel,
		@dwEXP)

	SET @dwCharSeq  = @@IDENTITY

	SELECT @bCreateCnt = COUNT(*) FROM TALLCHARTABLE WHERE dwUserID=@dwUserID AND bDelete=0
/*
	IF(@bCreateCnt > 0)
	BEGIN
		UPDATE TUSERINFOTABLE SET bCanCreateCharCount = bCanCreateCharCount -1 WHERE dwUserID = @dwUserID
		SET @bCreateCnt = @bCreateCnt - 1
	END
*/
	COMMIT TRAN ALLCHARTABLE


	RETURN @@ERROR

