



CREATE PROCEDURE [dbo].[TGuildArticleUpdate]
	@dwGuildID	INT,
	@dwID 		INT,
	@szTitle	VARCHAR(256),
	@szArticle	VARCHAR(2048)
AS

BEGIN TRAN GUILD_ARTICLE_UPDATE

	UPDATE TGUILDARTICLETABLE SET szTitle = @szTitle, szArticle = @szArticle WHERE dwGuildID = @dwGuildID AND dwID = @dwID

COMMIT TRAN GUILD_ARTICLE_UPDATE



