


CREATE PROCEDURE [dbo].[TSaveMoney]
	@dwCharID	INT,
	@dwGold	INT,
	@dwSilver	INT,
	@dwCooper	INT
AS

BEGIN TRAN SAVE_MONEY

	UPDATE TCHARTABLE SET dwGold = @dwGold, dwSilver = @dwSilver, dwCooper = @dwCooper WHERE dwCharID = @dwCharID

COMMIT TRAN SAVE_MONEY




