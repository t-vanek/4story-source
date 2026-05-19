

/* PROTECTED INSERT PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@dwProtected		INT
@szNAME		VARCHAR

========================================================
RETURN VALUE
========================================================
0: SUCCESS
1: NOT FOUND CHARACTER
2: ALREADY PROTECTED

========================================================
PROCESS
========================================================
1. Insert Protected Table

*/
CREATE PROCEDURE [dbo].[TProtectedInsert]
	@dwCharID 		INT,
	@dwProtected 		INT OUTPUT,
	@szNAME 		VARCHAR(50),
	@bOption		TINYINT
AS

SELECT @dwProtected = dwCharID FROM TCHARTABLE WHERE szNAME = @szNAME
IF(@@ROWCOUNT = 0)
BEGIN
	RETURN 1
END

SELECT dwCharID FROM TPROTECTEDTABLE WHERE dwCharID=@dwCharID AND dwProtected=@dwProtected
IF(@@ROWCOUNT != 0)
BEGIN
	RETURN 2
END

INSERT INTO TPROTECTEDTABLE(dwCharID, dwProtected, szName, bOption) VALUES(@dwCharID, @dwProtected, @szNAME,@bOption)

RETURN 0


