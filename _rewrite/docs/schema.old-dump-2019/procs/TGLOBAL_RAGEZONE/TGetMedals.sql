CREATE PROCEDURE [dbo].[TGetMedals]
@dwCharID INT,
@dwMedals INT OUTPUT
AS
SET @dwMedals = (SELECT dwMedals FROM TGAME_GSP.dbo.TMEDALS WHERE dwCharID = @dwCharID)

