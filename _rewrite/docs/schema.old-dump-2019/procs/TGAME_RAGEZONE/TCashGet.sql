

CREATE PROCEDURE [dbo].[TCashGet]
@dwUserID INT,
@dwCash INT OUTPUT,
@dwCashBonus INT OUTPUT
AS

EXEC TGLOBAL_GSP.DBO.TCashGet @dwUserID, @dwCash OUTPUT, @dwCashBonus OUTPUT


