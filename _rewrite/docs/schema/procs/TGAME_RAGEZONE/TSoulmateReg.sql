

CREATE PROCEDURE [dbo].[TSoulmateReg]
	@dwCharID	INT,
	@dwTarget	INT
AS

BEGIN TRAN SOULMATE_REG

	SELECT TOP 1 dwCharID FROM TSOULMATETABLE WHERE dwCharID = @dwCharID
	IF @@ROWCOUNT = 0
	BEGIN
		INSERT INTO TSOULMATETABLE 
		(
			dwCharID,
			dwTarget
		)
		VALUES
		(
			@dwCharID,
			@dwTarget
		)
	END
	ELSE
	BEGIN
		UPDATE 
			TSOULMATETABLE
		SET 
			dwTarget = @dwTarget, 
			dwTime = 0
		WHERE 
			dwCharID = @dwCharID
	END

COMMIT TRAN SOULMATE_REG



