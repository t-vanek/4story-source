

CREATE PROCEDURE [dbo].[TLoadService]
@bWorld tinyint,
@bType tinyint,
@szAddr varchar(50) output,
@wPort smallint output
AS

SET @szAddr=''
SET @wPort=0

EXEC TGLOBAL_GSP.DBO.TLoadService @bWorld, @bType, @szAddr output, @wPort output


