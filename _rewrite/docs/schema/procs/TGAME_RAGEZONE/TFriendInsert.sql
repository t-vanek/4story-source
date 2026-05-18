

/* FRIEND INSERT PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@dwFriendID		INT

========================================================
RETURN VALUE
========================================================

========================================================
PROCESS
========================================================
1. Insert Friend Table

*/
CREATE PROCEDURE [dbo].[TFriendInsert]
	@dwCharID 		INT,
	@dwFriendID 		INT
AS

BEGIN TRAN FRIENDINSERT
IF NOT EXISTS(SELECT dwCharID FROM TFRIENDTABLE WHERE dwCharID = @dwCharID AND dwFriendID = @dwFriendID)
BEGIN
	INSERT INTO TFRIENDTABLE VALUES(@dwCharID, @dwFriendID, 0)
END
COMMIT TRAN FRIENDINSERT



