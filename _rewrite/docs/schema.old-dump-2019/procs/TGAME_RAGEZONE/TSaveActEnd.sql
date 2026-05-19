
CREATE PROCEDURE [dbo].[TSaveActEnd]
@dwCharID INT
AS

UPDATE TCHARTABLE SET bStartAct = 0 WHERE dwCharID = @dwCharID


