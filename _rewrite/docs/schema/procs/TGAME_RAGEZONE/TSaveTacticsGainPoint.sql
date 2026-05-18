
CREATE PROCEDURE [dbo].[TSaveTacticsGainPoint]
@dwCharID INT,
@dwGainPoint INT
AS

UPDATE TGUILDTACTICSTABLE SET dwGainPoint = @dwGainPoint WHERE dwCharID=@dwCharID


