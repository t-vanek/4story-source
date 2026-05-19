
CREATE PROCEDURE [dbo].[TClearMapCurrentUser]
	@bGroupID	TINYINT,
	@bServerID	TINYINT,
	@bSvrType	TINYINT
 AS
 	EXEC TGLOBAL_GSP.dbo.TClearMapCurrentUser @bGroupID,@bServerID,@bSvrType


