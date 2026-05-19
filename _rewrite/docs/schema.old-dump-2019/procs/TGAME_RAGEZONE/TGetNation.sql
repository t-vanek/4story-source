

CREATE PROCEDURE [dbo].[TGetNation]
@bNation TINYINT OUTPUT
AS

SET @bNation = 0

EXEC TGLOBAL_GSP.DBO.TGetNation @bNation OUTPUT




