
CREATE PROCEDURE [dbo].[TGuildVolunteeringDel]
@bType TINYINT,
@dwCharID INT
AS

DELETE TGUILDVOLUNTEERTABLE WHERE dwCharID=@dwCharID AND bType=@bType


