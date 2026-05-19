


CREATE PROCEDURE [dbo].[TGuildContribution]
	@dwGuildID	INT,
	@dwCharID	INT,
	@dwExp	INT,
	@dwGold	INT,
	@dwSilver	INT,
	@dwCooper	INT
AS

BEGIN TRAN GUILD_CONTRIBUTION

	UPDATE TGUILDTABLE SET dwEXP=@dwExp, dwGold=@dwGold, dwSilver=@dwSilver, dwCooper=@dwCooper WHERE dwID = @dwGuildID

COMMIT TRAN GUILD_CONTRIBUTION


