

/* PROTECTED DELETE PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@dwProtected		INT

========================================================
RETURN VALUE
========================================================

========================================================
PROCESS
========================================================
1. Delete Protected Table

*/
CREATE PROCEDURE [dbo].[TProtectedDelete]
	@dwCharID 		INT,
	@dwProtected 		INT
AS

DELETE FROM TPROTECTEDTABLE WHERE (dwCharID=@dwCharID AND dwProtected= @dwProtected)



