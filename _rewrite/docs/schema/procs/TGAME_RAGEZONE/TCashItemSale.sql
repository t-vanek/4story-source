


CREATE PROCEDURE [dbo].[TCashItemSale] 
	@wID		SMALLINT,
	@bSaleValue	TINYINT
AS
	EXEC TGLOBAL_GSP.dbo.TCashItemSale @wID,@bSaleValue

	RETURN 0



