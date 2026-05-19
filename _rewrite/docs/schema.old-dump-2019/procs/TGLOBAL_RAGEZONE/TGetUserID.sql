CREATE PROCEDURE [dbo].[TGetUserID]
@szUserID varchar(50),
@szPasswd varchar(50),
@dwUserID int output
AS

exec  [192.168.1.9,6121].fourstory_ob.memlogin.csp_gamelogincheck @szUserID, @szPasswd, @dwUserID output

