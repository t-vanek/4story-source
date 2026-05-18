

CREATE PROCEDURE [dbo].[TTnmtEventEntry]
@wTourID SMALLINT,
@bEntryID TINYINT,
@szName VARCHAR(50),
@bType TINYINT,
@dwClass INT,
@dwFee INT,
@dwFeeBack INT,
@wPermitItemID SMALLINT,
@bPermitCount TINYINT,
@bMinLevel TINYINT,
@bMaxLevel TINYINT
AS

IF(@bEntryID = 0 )
BEGIN
	DELETE FROM TTNMTEVENTTABLE WHERE wTournamentID = @wTourID
	DELETE FROM TTNMTEVENTREWARDTABLE WHERE wTournamentID = @wTourID
END
ELSE
	INSERT INTO TTNMTEVENTTABLE(wTournamentID, bEntryID, szName, bType, dwClass, dwFee, dwFeeBack, wItemID, bItemCount, bMinLevel, bMaxLevel) VALUES(@wTourID,@bEntryID,@szName,@bType,@dwClass,@dwFee,@dwFeeBack,@wPermitItemID,@bPermitCount, @bMinLevel, @bMaxLevel)


