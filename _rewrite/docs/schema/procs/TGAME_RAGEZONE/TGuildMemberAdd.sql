




CREATE PROCEDURE [dbo].[TGuildMemberAdd]
	@dwGuildID 	INT,
	@dwCharID 	INT,
	@bLevel	TINYINT,
	@bDuty 	TINYINT
AS

BEGIN TRAN GUILD_MEMBER_ADD

	INSERT INTO 
		TGUILDMEMBERTABLE
	(
		dwCharID,
		dwGuildID,
		bDuty,
		bPeer,
		dwService
	)
	 VALUES
	(
		@dwCharID, 
		@dwGuildID,
		@bDuty,
		0,0
	)
/*
	DECLARE @dwScore INT
	SELECT @dwScore = dwScore FROM TLEVELCHART WHERE bLevel = @bLevel
	UPDATE TGUILDTABLE SET dwRankPoint = dwRankPoint + @dwScore, dwMemberCount = dwMemberCount+1 WHERE dwID = @dwGuildID
*/
COMMIT TRAN GUILD_MEMBER_ADD


