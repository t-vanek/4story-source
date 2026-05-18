/*
===============================
 RETURN VALUE
===============================
0 	:  Invalid Manager
그 외	:  bAuthority

===============================
*/

CREATE PROCEDURE [dbo].[TOPLogin]
	@szID		VARCHAR(50),
	@szPW		VARCHAR(50)

AS
	DECLARE @bAuthority	TINYINT
	
	SET @bAuthority = 0

	SELECT @bAuthority = bAuthority FROM TMANAGER WHERE szID = @szID AND szPasswd = @szPW
	
	IF(@@ROWCOUNT = 0 )
		RETURN 0

	RETURN @bAuthority

