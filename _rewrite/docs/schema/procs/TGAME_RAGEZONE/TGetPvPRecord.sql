

CREATE PROCEDURE [dbo].[TGetPvPRecord]
@dwCharID INT,
@dwUseablePoint INT OUTPUT,
@dwTotalPoint INT OUTPUT,
@dwRankOrder INT OUTPUT,
@bRankPercent TINYINT OUTPUT,
@dwWa_L INT OUTPUT,
@dwWa_W INT OUTPUT,
@dwRa_L INT OUTPUT,
@dwRa_W INT OUTPUT,
@dwAr_L INT OUTPUT,
@dwAr_W INT OUTPUT,
@dwWi_L INT OUTPUT,
@dwWi_W INT OUTPUT,
@dwPr_L INT OUTPUT,
@dwPr_W INT OUTPUT,
@dwSo_L INT OUTPUT,
@dwSo_W INT OUTPUT
AS

SET @dwUseablePoint = 0
SET @dwTotalPoint = 0
SET @dwRankOrder = 0
SET @bRankPercent = 0
SET @dwWa_W = 0
SET @dwWa_L= 0
SET @dwRa_W = 0
SET @dwRa_L = 0
SET @dwAr_W = 0
SET @dwAr_L = 0
SET @dwWi_W = 0
SET @dwWi_L = 0
SET @dwPr_W = 0
SET @dwPr_L = 0
SET @dwSo_W = 0
SET @dwSo_L = 0

SELECT @dwUseablePoint = dwUseablePoint, @dwTotalPoint=dwTotalPoint FROM TPVPOINTTABLE WHERE dwCharID=@dwCharID
IF(@@ROWCOUNT > 0 AND @dwTotalPoint > 0)
BEGIN
	SELECT @dwRankOrder = COUNT(dwCharID)+1 FROM TPVPOINTTABLE WHERE dwTotalPoint > @dwTotalPoint 
	SELECT @bRankPercent = @dwRankOrder / COUNT(dwCharID) * 100  FROM TPVPOINTTABLE

	IF(@bRankPercent = 0)
		SET @bRankPercent = 1
END

SELECT @dwWa_W=dwWarrior_win,
	@dwWa_L = dwWarrior_lose,
	@dwRa_W=dwRanger_win,
	@dwRa_L=dwRanger_lose,
	@dwAr_W=dwArcher_win,
	@dwAr_L=dwArcher_lose,
	@dwWi_W=dwWizard_win,
	@dwWi_L=dwWizard_lose,
	@dwPr_W=dwPriest_win,
	@dwPr_L=dwPriest_lose,
	@dwSo_W=dwSorcerer_win,
	@dwSo_L=dwSorcerer_lose
	FROM TPVPRECORDTABLE WHERE dwCharID = @dwCharID



