/*
	Server Start / Down시 호출 됨

*/
CREATE PROCEDURE [dbo].[OPTool_SMSEmergency] 
	@pServerType	TINYINT,
	@pServerID	INT,
	@pServerStatus	TINYINT
AS
/*	DECLARE @szPhoneNum	VARCHAR (50)	
	DECLARE @szMessage		VARCHAR (50)

	DECLARE CUR_SMS CURSOR FOR 	
		SELECT szPhoneNum
		FROM	TMANAGER
		WHERE LEN(szPhoneNum) > 0


	SET	@szMessage = '4S Emergency ' 

	IF @pServerType = 1
	BEGIN
		SET @szMessage = @szMessage + 'Login( '
	END
	ELSE IF @pServerType = 2
	BEGIN
		SET @szMessage = @szMessage + 'Map( '
	END
	ELSE IF @pServerType = 3
	BEGIN
		SET @szMessage = @szMessage + 'World( '
	END
	
	SET	@szMessage = @szMessage + CONVERT( VARCHAR, @pServerID ) + ' ) '

	IF @pServerStatus = 1
	BEGIN
		SET	@szMessage = @szMessage + '- Start '
	END 
	ELSE IF @pServerStatus = 2
	BEGIN
		SET	@szMessage = @szMessage + '- Down '
	END


	SET	@szMessage = @szMessage + 'at ' +  CONVERT( VARCHAR(19), GETDATE(), 121 )


	OPEN CUR_SMS
	
	FETCH NEXT FROM CUR_SMS INTO  @szPhoneNum

	WHILE @@FETCH_STATUS = 0
	BEGIN


		--	EXEC SMS.DBO.Proc_SMS @szPhoneNum, @szMessage

		FETCH NEXT FROM CUR_SMS INTO  @szPhoneNum

	END
	
	CLOSE CUR_SMS

	DEALLOCATE CUR_SMS
*/

