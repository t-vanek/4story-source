


CREATE PROCEDURE [dbo].[TGuildFame]
	@dwGuildID	INT,
	@dwFame	INT,
	@dwFameColor	INT
AS

BEGIN TRAN GUILD_FAME

	UPDATE TGUILDTABLE SET dwFame = @dwFame, dwFameColor = @dwFameColor WHERE dwID = @dwGuildID

COMMIT TRAN GUILD_FAME




