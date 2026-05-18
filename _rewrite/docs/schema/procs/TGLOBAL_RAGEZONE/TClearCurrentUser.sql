
CREATE PROCEDURE [dbo].[TClearCurrentUser]
@dwUserID int
AS

DECLARE @bLocked TINYINT
SELECT @bLocked = bLocked FROM TCURRENTUSER WHERE dwUserID = @dwUserID
IF(@bLocked = 1)
	DELETE TCURRENTUSER WHERE dwUserID = @dwUserID


