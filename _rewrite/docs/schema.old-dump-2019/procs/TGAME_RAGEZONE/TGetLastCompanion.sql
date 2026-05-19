CREATE PROCEDURE [dbo].[TGetLastCompanion]
@dwCharID INT,
@bCompanionSlot INT OUTPUT
AS
BEGIN
SELECT @bCompanionSlot = bCompanionSlot FROM TLASTCOMPANIONTABLE WHERE dwCharID = @dwCharID
END
