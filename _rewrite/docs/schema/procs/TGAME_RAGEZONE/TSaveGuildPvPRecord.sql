

CREATE PROCEDURE [dbo].[TSaveGuildPvPRecord]
@dwGuildID INT,
@dwCharID INT,
@dwDate INT,
@wKillCount SMALLINT,
@wDieCount SMALLINT,
@dwPoint_1 INT,
@dwPoint_2 INT,
@dwPoint_3 INT,
@dwPoint_4 INT,
@dwPoint_5 INT,
@dwPoint_6 INT,
@dwPoint_7 INT,
@dwPoint_8 INT
AS

DELETE TGUILDPVPRECORDTABLE WHERE dwGuildID = @dwGuildID AND dwCharID=@dwCharID AND (dwDate + 7 <= @dwDate OR dwDate = @dwDate)
INSERT INTO TGUILDPVPRECORDTABLE (dwGuildID, dwCharID, dwDate, wKillCount, wDieCount, dwPoint_1, dwPoint_2, dwPoint_3, dwPoint_4, dwPoint_5, dwPoint_6, dwPoint_7, dwPoint_8) VALUES(
	@dwGuildID, @dwCharID,@dwDate ,@wKillCount ,@wDieCount ,@dwPoint_1 ,@dwPoint_2 ,@dwPoint_3 ,@dwPoint_4 ,@dwPoint_5 ,@dwPoint_6 ,@dwPoint_7 ,@dwPoint_8)



