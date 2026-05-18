
CREATE PROCEDURE [dbo].[TItemDelete]
@dwOwnerID INT,
@bOwnerType TINYINT,
@bStorageType TINYINT,
@dwStorageID INT
AS

DELETE TITEMTABLE WHERE dwOwnerID=@dwOwnerID AND bOwnerType=@bOwnerType AND bStorageType=@bStorageType AND dwStorageID=@dwStorageID


