CREATE PROCEDURE [dbo].[TGetPlayTime]
@dwCharID INT,
@dwPlayTime INT OUTPUT
AS
SET @dwPlayTime = (SELECT dwPlayTime FROM TPLAYTIMETABLE WHERE dwCharID = @dwCharID)

