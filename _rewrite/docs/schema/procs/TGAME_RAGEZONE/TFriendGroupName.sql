

/* FRIEND GROUP NAME PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@bGroup		TINYINT	OUTPUT

========================================================
RETURN VALUE
========================================================

========================================================
PROCESS
========================================================
1. Check Group
2. Change friend group data name

*/
CREATE PROCEDURE [dbo].[TFriendGroupName]
	@dwCharID		INT,
	@bGroup		TINYINT,
	@szNAME		VARCHAR(20)
AS

UPDATE TFRIENDGROUPTABLE SET szNAME = @szNAME WHERE (dwCharID = @dwCharID) and (bGroup = @bGroup)



