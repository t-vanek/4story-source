
CREATE PROCEDURE [dbo].[TDelMountSaddle_copy]
	@dwUserID 	INT
AS

DELETE TMOUNTITEMTABLE WHERE dwUserID = @dwUserID


