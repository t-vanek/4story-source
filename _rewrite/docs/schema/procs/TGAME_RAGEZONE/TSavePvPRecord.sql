

CREATE PROCEDURE [dbo].[TSavePvPRecord]
@dwCharID INT,
@dwPvPUseablePoint INT,
@dwPvPTotalPoint INT,
@dwWa_L INT ,
@dwWa_W INT ,
@dwRa_L INT ,
@dwRa_W INT ,
@dwAr_L INT ,
@dwAr_W INT ,
@dwWi_L INT ,
@dwWi_W INT ,
@dwPr_L INT ,
@dwPr_W INT ,
@dwSo_L INT ,
@dwSo_W INT
AS

BEGIN TRAN SAVEPVPRECORD
IF EXISTS(SELECT dwCharID FROM TPVPOINTTABLE WHERE dwCharID = @dwCharID)
	UPDATE TPVPOINTTABLE SET dwUseablePoint = @dwPvPUseablePoint, dwTotalPoint = @dwPvPTotalPoint WHERE dwCharID=@dwCharID
ELSE
	INSERT INTO TPVPOINTTABLE (dwCharID, dwUseablePoint, dwTotalPoint) VALUES(@dwCharID, @dwPvPUseablePoint, @dwPvPTotalPoint)
COMMIT TRAN SAVEPVPRECORD

IF EXISTS( SELECT dwCharID FROM TPVPRECORDTABLE WHERE dwCharID = @dwCharID)
	UPDATE TPVPRECORDTABLE SET 
		dwWarrior_win = @dwWa_W,
		dwWarrior_lose = @dwWa_L,
		dwRanger_win = @dwRa_W,
		dwRanger_lose = @dwRa_L,
		dwArcher_win = @dwAr_W,
		dwArcher_lose = @dwAr_L,
		dwWizard_win = @dwWi_W,
		dwWizard_lose = @dwWi_L,
		dwPriest_win = @dwPr_W,
		dwPriest_lose = @dwPr_L,
		dwSorcerer_win = @dwSo_W,
		dwSorcerer_lose = @dwSo_L
		 WHERE dwCharID = @dwCharID
ELSE
	INSERT INTO TPVPRECORDTABLE (
		dwCharID,
		dwWarrior_win,
		dwWarrior_lose,
		dwRanger_win,
		dwRanger_lose,
		dwArcher_win,
		dwArcher_lose,
		dwWizard_win,
		dwWizard_lose,
		dwPriest_win,
		dwPriest_lose,
		dwSorcerer_win,
		dwSorcerer_lose) VALUES(
		@dwCharID,
		@dwWa_W,
		@dwWa_L,
		@dwRa_W,
		@dwRa_L,
		@dwAr_W,
		@dwAr_L,
		@dwWi_W,
		@dwWi_L,
		@dwPr_W,
		@dwPr_L,
		@dwSo_W,
		@dwSo_L)



