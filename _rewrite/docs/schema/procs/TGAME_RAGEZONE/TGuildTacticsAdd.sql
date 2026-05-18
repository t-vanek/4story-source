


CREATE PROCEDURE [dbo].[TGuildTacticsAdd]
@dwGuildID INT,
@dwCharID INT,
@dwPoint INT,
@dlMoney BIGINT,
@bDay TINYINT,
@dEndTime SMALLDATETIME,
@dwCharGuildID INT OUTPUT
AS

SET @dwCharGuildID = 0

SELECT @dwCharGuildID = dwGuildID FROM TGUILDMEMBERTABLE WHERE dwCharID=@dwCharID

IF EXISTS( SELECT dwCharID FROM TGUILDTACTICSTABLE WHERE dwCharID=@dwCharID)
	UPDATE TGUILDTACTICSTABLE SET dwGuildID=@dwGuildID, dwRewardPoint=@dwPoint, dlRewardMoney=@dlMoney, bDay=@bDay, dEndTime=@dEndTime, dwGainPoint=0 WHERE dwCharID=@dwCharID
ELSE
	INSERT INTO TGUILDTACTICSTABLE (
		dwCharID, dwGuildID, dwRewardPoint, dlRewardMoney, dwGainPoint, bDay, dEndTime) VALUES(
		@dwCharID, @dwGuildID, @dwPoint, @dlMoney, 0, @bDay, @dEndTime)


