
CREATE PROCEDURE [dbo].[TTnmtEventReward]
@wTourID SMALLINT,
@bEntryID TINYINT,
@bChartType TINYINT,
@wItemID SMALLINT,
@bCount TINYINT,
@dwClass INT,
@bShield TINYINT
AS

INSERT INTO TTNMTEVENTREWARDTABLE(wTournamentID, bEntryID, bChartType, wItemID, bItemCount, dwClass, bCheckShield) VALUES(@wTourID,@bEntryID,@bChartType,@wItemID,@bCount,@dwClass,@bShield)


