/* CHECK PASSWD PROCESS

========================================================
PARAMETER
========================================================
@dwUserID		INT
@szPasswd		VARCHAR(50)

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: INVALID PASSWD

========================================================
PROCESS
========================================================
1. Check Passwd

*/


CREATE PROCEDURE [dbo].[TCheckPasswd]
	@dwUserID		INT,
	@szPasswd		VARCHAR(50)
AS
	DECLARE @nReturn int
	DECLARE @szPasswordDB VARCHAR(50)

	SELECT @szPasswordDB = szPasswd FROM TACCOUNT_PW WHERE dwUserID = @dwUserID

	IF(@szPasswd = @szPasswordDB)
	BEGIN
		RETURN 0
	END

	RETURN 1

