
CREATE PROCEDURE [dbo].[TUpdateCharMoney]
@bWorldID TINYINT,
@dwCharID INT,
@dwGold INT,
@dwSilver INT,
@dwCooper INT
AS

UPDATE TALLCHARTABLE SET dwGold=@dwGold, dwSilver=@dwSilver, dwCooper = @dwCooper WHERE dwCharID=@dwCharID AND bWorldID=@bWorldID

