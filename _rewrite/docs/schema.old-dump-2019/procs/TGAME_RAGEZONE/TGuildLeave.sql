


CREATE PROCEDURE [dbo].[TGuildLeave]
	@dwGuildID		INT,
	@dwCharID 		INT,
	@bLeave 		TINYINT,
	@dwLeaveTime		INT
AS

BEGIN TRAN GUILD_LEAVE

	DELETE TGUILDMEMBERTABLE WHERE dwCharID = @dwCharID AND dwGuildID = @dwGuildID
	DELETE TGUILDPVPRECORDTABLE WHERE dwGuildID=@dwGuildID AND dwCharID = @dwCharID
	UPDATE TCHARTABLE SET bGuildLeave=@bLeave , dwGuildLeaveTime=@dwLeaveTime WHERE dwCharID = @dwCharID
/*
	DECLARE @bPrevLevel TINYINT
	DECLARE @dwPrevScore INT
	
	SELECT @bPrevLevel = bLevel FROM TCHARTABLE WHERE dwCharID = @dwCharID
	SELECT @dwPrevScore = dwScore FROM TLEVELCHART WHERE bLevel = @bPrevLevel
	UPDATE TGUILDTABLE SET dwRankPoint = dwRankPoint - @dwPrevScore, dwMemberCount = dwMemberCount-1 WHERE dwID = @dwGuildID
*/
COMMIT TRAN GUILD_LEAVE



