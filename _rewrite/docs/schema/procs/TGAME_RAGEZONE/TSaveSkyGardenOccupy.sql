


CREATE PROCEDURE [dbo].[TSaveSkyGardenOccupy]
@bCountry TINYINT,
@wID SMALLINT,
@bType TINYINT
AS

UPDATE TSKYGARDENTABLE SET bCountry = @bCountry



