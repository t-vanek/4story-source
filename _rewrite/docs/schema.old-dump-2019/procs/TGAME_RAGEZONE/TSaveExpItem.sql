



CREATE PROCEDURE [dbo].[TSaveExpItem]
@dwCharID int,
@wItemID smallint,
@bType tinyint,
@dwRemainTime int,
@dEndTime smalldatetime
AS

BEGIN TRAN SAVE_EXPITEM

INSERT INTO TTEMPEXPITEMTABLE(dwCharID, wItemID, bType, dwRemainTime, dEndTime) VALUES(@dwCharID, @wItemID, @bType, @dwRemainTime, @dEndTime)

COMMIT TRAN SAVE_EXPITEM



