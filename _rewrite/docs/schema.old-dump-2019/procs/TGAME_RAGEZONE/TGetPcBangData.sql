

CREATE PROCEDURE [dbo].[TGetPcBangData]
@dwUserID		INT,
@dwPlayDate		INT,
@bInPcBang		TINYINT OUTPUT,
@dwPcBangTime	INT OUTPUT,
@bItemCnt		TINYINT OUTPUT,
@bLuckyNumber	TINYINT OUTPUT
AS

DECLARE @dwPcBang INT
DECLARE @bItemKind TINYINT

SET @bInPcBang = 0
SET @dwPcBangTime = 0
SET @bItemCnt = 0

IF EXISTS(SELECT dwUserID FROM TFAKECHARCHART WHERE @dwUserID = dwUserID)
	RETURN 0

SELECT @dwPcBang = dwPcBangID, @bLuckyNumber = bLuckyNumber FROM TGLOBAL_GSP.dbo.TCURRENTUSER WHERE dwUserID = @dwUserID
IF(@@ROWCOUNT <> 0 AND @dwPcBang <> 0)
	SET @bInPcBang = 1

SELECT @bItemKind = bKind FROM TITEMCHART WHERE wItemID = (SELECT wItemID FROM TVIEW_DURINGITEMTABLE WHERE dwUserID = @dwUserID)
IF(@bItemKind = 40)
	SET @bInPcBang = @bInPcBang | 2
ELSE IF(@bItemKind = 52)
	SET @bInPcBang = @bInPcBang | 4

IF(@bInPcBang  <> 0)
	SELECT @dwPcBangTime = dwPlayTime, @bItemCnt = bItemCnt  FROM TGLOBAL_GSP.DBO.TPCBANGPLAYTABLE WHERE dwUserID = @dwUserID AND dwPlayDate = @dwPlayDate


