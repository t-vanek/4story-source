CREATE PROCEDURE [dbo].[TDeleteAllQuest]
@szNAME varchar(50)
AS
DECLARE @dwCharID INT

SELECT @dwCharID = dwCharID FROM TCHARTABLE WHERE szName = @szNAME
IF(@@ROWCOUNT != 1)
	RETURN 'Can Find Char'

DELETE TQUESTTABLE WHERE dwCharID = @dwCharID



