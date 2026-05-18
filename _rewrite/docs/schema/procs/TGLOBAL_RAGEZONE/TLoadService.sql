CREATE PROCEDURE [dbo].[TLoadService]
@bWorld tinyint,
@bServiceGroup tinyint,
@szIP varchar(50) output,
@wPort smallint output
AS

DECLARE @bMachineID tinyint

SET @szIP = ''
SET @wPort = 0

SELECT @wPort = wPort, @bMachineID = bMachineID FROM TSERVER WHERE @bWorld = bGroupID AND @bServiceGroup = bType

SELECT @szIP = szPriAddr FROM TIPADDR WHERE @bMachineID = bMachineID


