


CREATE PROCEDURE [dbo].[TGuildKickout]
	@dwGuildID	INT,
	@dwCharID 	INT
AS

BEGIN TRAN GUILD_KICKOUT

	DELETE TGUILDMEMBERTABLE WHERE dwCharID = @dwCharID AND dwGuildID = @dwGuildID
	DELETE TGUILDPVPRECORDTABLE WHERE dwGuildID=@dwGuildID AND dwCharID = @dwCharID
	UPDATE TCHARTABLE SET bGuildLeave=0 , dwGuildLeaveTime=0 WHERE dwCharID = @dwCharID
/*
	DECLARE @bPrevLevel TINYINT
	DECLARE @dwPrevScore INT
	
	SELECT @bPrevLevel = bLevel FROM TCHARTABLE WHERE dwCharID = @dwCharID
	SELECT @dwPrevScore = dwScore FROM TLEVELCHART WHERE bLevel = @bPrevLevel
	UPDATE TGUILDTABLE SET dwRankPoint = dwRankPoint - @dwPrevScore, dwMemberCount = dwMemberCount-1 WHERE dwID = @dwGuildID
*/
COMMIT TRAN GUILD_KICKOUT



