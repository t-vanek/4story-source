
CREATE PROCEDURE [dbo].[TTnmtEventSchedule]
@wTourID SMALLINT,
@bStep TINYINT,
@dwPeriod INT
AS

INSERT INTO  TTNMTEVENTSCHEDULETABLE(wTournamentID, bStep, dwPeriod) VALUES(@wTourID, @bStep, @dwPeriod)


