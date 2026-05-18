
CREATE PROCEDURE [dbo].[TSaveTempDuringItem]
@dwUserID int,
@wItemID smallint,
@bType tinyint,
@dwRemainTime int,
@dEndTime smalldatetime
AS

INSERT INTO TTEMPDURINGITEMTABLE (dwUserID, wItemID, bType, dwRemainTime, dEndTime) VALUES(@dwUserID, @wItemID, @bType, @dwRemainTime, @dEndTime)


