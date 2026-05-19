
CREATE PROCEDURE [dbo].[TGetMountSaddle_copy]
	@dwUserID 	INT,
	@wItemID	SMALLINT		OUTPUT,
	@tEndTime	SMALLDATETIME	OUTPUT,
  @bType TINYINT OUTPUT
AS

SELECT @wItemID = wItemID, @tEndTime = dEndTime, @bType = bType FROM TMOUNTITEMTABLE WHERE dwUserID = @dwUserID
IF(@@ROWCOUNT = 0)
BEGIN
	SET @wItemID = 0
	SET @tEndTime = 0
  SET @bType = 0
END

IF(GETDATE() > @tEndTime and @bType = 0)
BEGIN
	DELETE TMOUNTITEMTABLE WHERE dwUserID = @dwUserID
	SET @wItemID = 0
	SET @tEndTime = 0
  SET @bType = 0
END


