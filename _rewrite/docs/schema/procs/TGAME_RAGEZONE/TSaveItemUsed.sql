


CREATE PROCEDURE [dbo].[TSaveItemUsed]
@dwCharID int,
@wDelayGroupID smallint,
@dwTick int
AS

BEGIN TRAN TEMPITEMUSED

INSERT INTO TTEMPITEMUSEDTABLE(
	dwCharID,
	wDelayGroupID,
	dwTick) VALUES(
	@dwCharID,
	@wDelayGroupID,
	@dwTick)

COMMIT TRAN TEMPITEMUSED


