

CREATE PROCEDURE [dbo].[TSaveGuildPvPoint]
@dwGuildID INT,
@dwTotalPoint INT,
@dwUseablePoint INT,
@dwMonthPoint INT
AS

UPDATE TGUILDTABLE SET dwPvPTotalPoint = @dwTotalPoint, dwPvPUseablePoint = @dwUseablePoint, dwPvPMonthPoint=@dwMonthPoint WHERE dwID = @dwGuildID

