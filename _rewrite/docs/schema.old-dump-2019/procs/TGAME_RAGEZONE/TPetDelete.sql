



CREATE PROCEDURE [dbo].[TPetDelete]
@dwUserID INT,
@wPetID SMALLINT
AS

DELETE TPETTABLE WHERE dwUserID = @dwUserID AND wPetID = @wPetID 






