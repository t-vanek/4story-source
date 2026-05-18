

/* FRIEND GROUP MAKE PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@szNAME		VARCHAR(20)
@bGroup		TINYINT

========================================================
RETURN VALUE
========================================================

========================================================
PROCESS
========================================================
1. Check Group
2. Insert group data

*/
CREATE PROCEDURE [dbo].[TFriendGroupMake]
	@dwCharID		INT,
	@bGroup		TINYINT,
	@szNAME		VARCHAR(20)
AS

INSERT INTO TFRIENDGROUPTABLE(dwCharID, bGroup, szNAME)  VALUES(@dwCharID, @bGroup, @szName)



