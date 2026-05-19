

/* PROTECTED SEARCH PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@dwProtected		INT

========================================================
RETURN VALUE
========================================================
0 		: SUCCESS
1 		: PROTECTED

========================================================
PROCESS
========================================================
1. Check protected table
2. Return result

*/
CREATE PROCEDURE [dbo].[TProtectedSearch]
	@dwCharID		INT,
	@dwProtected		INT
AS

SELECT dwCharID FROM TPROTECTEDTABLE WHERE (dwCharID = @dwCharID) and (dwProtected = @dwProtected)
IF @@ROWCOUNT > 0
	RETURN 1
ELSE
	RETURN 0



