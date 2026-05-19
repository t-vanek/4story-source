

CREATE PROCEDURE [dbo].[TGuildDisorg]
@dwGuildID int,
@bDisorg tinyint,
@dwTime int
AS

BEGIN TRAN GUILD_DISORG
UPDATE TGUILDTABLE SET bDisorg=@bDisorg , dwTime=@dwTime WHERE dwID=@dwGuildID
COMMIT TRAN GUILD_DISORG



