

CREATE PROCEDURE [dbo].[TCharSetAsk]
	@szName 	VARCHAR(50),
	@dwGold	INT,
	@dwSilver	INT,
	@dwCooper	INT
 AS

DECLARE 	@dwID 		INT
DECLARE 	@bClass 	TINYINT
DECLARE 	@bCountry 	TINYINT

SELECT @dwID = dwCharID, @bClass = bClass, @bCountry = bCountry FROM TCHARTABLE WHERE szName = @szName
IF @@ROWCOUNT = 0
BEGIN
	PRINT 1
	RETURN
END

/*
	UPDATE TCHARTABLE_PW SET fPosX = 4589, fPosY = 77, fPosZ = 5699 WHERE dwCharID = @dwID
ELSE IF @bCountry = 0
	UPDATE TCHARTABLE_PW SET fPosX = 3167, fPosY = 116, fPosZ = 5725 WHERE dwCharID = @dwID */

UPDATE TCHARTABLE SET dwGold = @dwGold + dwGold, dwSilver = @dwSilver + dwSilver, dwCooper = @dwCooper + dwCooper WHERE dwCharID = @dwID


