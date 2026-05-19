



CREATE PROCEDURE [dbo].[TSavePet]
@dwCharID INT,
@wPetID SMALLINT,
@strPetName VARCHAR(50),
@timeUse SMALLDATETIME,
@bEffect TINYINT
AS
DECLARE @dwUserID INT -- try it

SET @dwUserID = (SELECT dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID)

BEGIN TRAN TSAVEMOUNT
IF NOT EXISTS(SELECT TOP 1 @wPetID FROM TPETTABLE WHERE dwUserID = @dwUserID AND wPetID = @wPetID)
BEGIN
	INSERT INTO TPETTABLE(dwUserID, wPetID, szName, timeUse, bEffect) 
	VALUES(@dwUserID, @wPetID, @strPetName, @timeUse, @bEffect)
END
ELSE
BEGIN
	UPDATE TPETTABLE SET
		szName = @strPetName, timeUse = @timeUse, bEffect = @bEffect
	WHERE dwUserID = @dwUserID AND wPetID = @wPetID
END
COMMIT TRAN TSAVEMOUNT



