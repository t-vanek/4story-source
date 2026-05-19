CREATE PROCEDURE [dbo].[TGetBattleRankPoint]
@dwCharID INT,
@dwRankPoint INT OUTPUT
AS
SET @dwRankPoint = (SELECT dwRankPoint FROM TRANKING WHERE dwCharID = @dwCharID)

