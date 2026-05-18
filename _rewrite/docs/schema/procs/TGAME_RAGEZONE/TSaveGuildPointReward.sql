

CREATE PROCEDURE [dbo].[TSaveGuildPointReward]
@dwGuildID INT,
@dwPoint INT,
@szName VARCHAR(50),
@dwTotalPoint INT,
@dwUseablePoint INT
AS

DECLARE @dwCharID INT

SELECT @dwCharID = dwCharID FROM TCHARTABLE WHERE szName=@szName
IF(@@ROWCOUNT = 0)
	RETURN 1

UPDATE TGUILDTABLE SET dwPvPUseablePoint = @dwUseablePoint, dwPvPTotalPoint = @dwTotalPoint WHERE dwID = @dwGuildID
INSERT INTO TGUILDPVPOINTREWARDTABLE (dwGuildID, szName, dwPoint, dlDate) VALUES(@dwGuildID, @szName, @dwPoint, GETDATE())

BEGIN TRAN SAVEGUILDPOINTREWARD
UPDATE TPVPOINTTABLE SET dwUseablePoint = dwUseablePoint + @dwPoint WHERE dwCharID = @dwCharID
COMMIT TRAN SAVEGUILDPOINTREWARD



