


CREATE PROCEDURE [dbo].[TSaveMissionOccupy]
@wLocalID SMALLINT,
@bType TINYINT,
@dwCharID INT,
@bCountry TINYINT
AS

UPDATE TMISSIONTABLE SET bCountry = @bCountry  WHERE wMissionID = @wLocalID


