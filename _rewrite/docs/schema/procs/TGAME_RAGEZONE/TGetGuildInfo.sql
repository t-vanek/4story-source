

CREATE PROCEDURE [dbo].[TGetGuildInfo]
@dwCharID INT,
@szName VARCHAR(50) OUTPUT,
@dwFame  INT OUTPUT,
@dwFameColor INT OUTPUT
AS

DECLARE @dwGuildID INT

SET @dwGuildID = 0
SET @dwFame = 0
SET @dwFameColor = 0

SELECT @dwGuildID = dwGuildID FROM TGUILDMEMBERTABLE WHERE dwCharID=@dwCharID
IF(@dwGuildID > 0)
	SELECT @szName=szName, @dwFame = dwFame, @dwFameColor = dwFameColor FROM TGUILDTABLE WHERE dwID = @dwGuildID




