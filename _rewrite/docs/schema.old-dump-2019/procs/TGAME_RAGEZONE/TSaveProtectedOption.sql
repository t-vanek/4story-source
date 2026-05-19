
CREATE PROCEDURE [dbo].[TSaveProtectedOption]
@dwCharID INT,
@dwTarget INT,
@bOption TINYINT
AS

IF EXISTS( SELECT dwCharID FROM TPROTECTEDTABLE WHERE dwCharID=@dwCharID AND dwProtected = @dwTarget)
	UPDATE TPROTECTEDTABLE SET bOption = @bOption WHERE dwCharID=@dwCharID AND dwProtected = @dwTarget


