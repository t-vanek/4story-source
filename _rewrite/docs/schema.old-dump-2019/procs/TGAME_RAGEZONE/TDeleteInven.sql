

/* DELETE INVEN PROCESS

========================================================
PARAMETER
========================================================
@dwCharID	INT
@bInvenID	TINYINT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS

========================================================
PROCESS
========================================================
1. Delete Item
2. Delete Inven

*/


CREATE PROCEDURE [dbo].[TDeleteInven]
	@dwCharID	INT,
	@bInvenID	TINYINT
AS
	DELETE FROM TITEMTABLE WHERE dwOwnerID = @dwCharID AND bOwnerType=0 AND bStorageType=0 AND dwStorageID = @bInvenID
	DELETE FROM TINVENTABLE WHERE dwCharID = @dwCharID AND bInvenID = @bInvenID

	RETURN 0


