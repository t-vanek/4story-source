



CREATE PROCEDURE [dbo].[TGuildArticleAdd]
	@dwGuildID	INT,
	@dwID 		INT,
	@bDuty		TINYINT,
	@szWritter	VARCHAR(50),
	@szTitle	VARCHAR(256),
	@szArticle	VARCHAR(2048),
	@dwTime	INT
AS

BEGIN TRAN GUILD_ARTICLE_ADD

	INSERT INTO TGUILDARTICLETABLE VALUES(@dwGuildID, @dwID, @bDuty, @szWritter, @szTitle, @szArticle, @dwTime)

COMMIT TRAN GUILD_ARTICLE_ADD



