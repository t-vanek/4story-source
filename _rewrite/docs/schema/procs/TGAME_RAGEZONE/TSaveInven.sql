





/* SAVE INVEN PROCESS

========================================================
PARAMETER
========================================================
@dwCharID	INT
@bInvenID	TINYINT
@wItemID	SMALLINT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS

========================================================
PROCESS
========================================================
1. Check Inven
2. Insert or Update Inven

*/


CREATE PROCEDURE [dbo].[TSaveInven]
	@dwCharID	INT,
	@bInvenID	TINYINT,
	@wItemID	SMALLINT,
	@dEndTime	SMALLDATETIME,
	@bELD		TINYINT
AS
BEGIN TRAN TSAVEINVEN

	INSERT INTO TTEMPINVENTABLE (dwCharID, bInvenID, wItemID, dEndTime, bELD) VALUES(@dwCharID, @bInvenID, @wItemID, @dEndTime, @bELD)

COMMIT TRAN TSAVEINVEN
RETURN 0


