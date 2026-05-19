CREATE PROCEDURE [dbo].[TGetMedals]
@dwCharID INT,
@dwMedals INT OUTPUT
AS
SET @dwMedals = (SELECT dwMedals FROM TMEDALS WHERE dwCharID = @dwCharID)

