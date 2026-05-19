


CREATE PROCEDURE [dbo].[TSaveDuringItem]
@dwCharID int,
@wItemID smallint,
@bType tinyint,
@dwRemainTime int,
@dEndTime smalldatetime
AS

DECLARE @dwUserID INT
SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID

EXEC TGLOBAL_GSP.DBO.TSaveTempDuringItem @dwUserID, @wItemID, @bType, @dwRemainTime, @dEndTime




