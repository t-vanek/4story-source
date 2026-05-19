

/* LOGOUT PROCESS

========================================================
PARAMETER
========================================================
@dwUserID		INT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: NO USER

========================================================
PROCESS
========================================================
1. Check TCURRENTUSER table
2. Delete user from TCURRENTUSER
3. Update log data

*/

CREATE PROCEDURE [dbo].[TLogout]
	@dwUserID	INT,
	@dwCharID	INT
AS
	DECLARE @nResult	INT
	DECLARE @bLevel	TINYINT
	DECLARE @dwExp	INT
	DECLARE @dwPlayTime INT
	DECLARE @dwGold	INT
	DECLARE @dwSilver	INT
	DECLARE @dwCooper	INT
	DECLARE @bWorldID	TINYINT
	DECLARE @dCurDate SMALLDATETIME
	DECLARE @bSave TINYINT
	DECLARE @bInven TINYINT
	DECLARE @bType1 TINYINT
	DECLARE @wID1 SMALLINT
	DECLARE @bType2 TINYINT
	DECLARE @wID2 SMALLINT
	DECLARE @bType3 TINYINT
	DECLARE @wID3 SMALLINT
	DECLARE @bType4 TINYINT
	DECLARE @wID4 SMALLINT
	DECLARE @bType5 TINYINT
	DECLARE @wID5 SMALLINT
	DECLARE @bType6 TINYINT
	DECLARE @wID6 SMALLINT
	DECLARE @bType7 TINYINT
	DECLARE @wID7 SMALLINT
	DECLARE @bType8 TINYINT
	DECLARE @wID8 SMALLINT
	DECLARE @bType9 TINYINT
	DECLARE @wID9 SMALLINT
	DECLARE @bType10 TINYINT
	DECLARE @wID10 SMALLINT
	DECLARE @bType11 TINYINT
	DECLARE @wID11 SMALLINT
	DECLARE @bType12 TINYINT
	DECLARE @wID12 SMALLINT
	


IF(@bSave = 2)
	INSERT INTO THOTKEYTABLE VALUES(@dwCharID, @bInven, @bType1, @wID1, @bType2, @wID2, @bType3, @wID3, @bType4, @wID4,
						 @bType5, @wID5, @bType6, @wID6, @bType7, @wID7, @bType8, @wID8, @bType9, @wID9,
						 @bType10, @wID10, @bType11, @wID11, @bType12, @wID12)
ELSE IF(@bSave = 3)
	UPDATE THOTKEYTABLE SET bType1=@bType1, wID1=@wID1, bType2=@bType2, wID2=@wID2, bType3=@bType3, wID3=@wID3, bType4=@bType4, wID4=@wID4
					, bType5=@bType5, wID5=@wID5, bType6=@bType6, wID6=@wID6, bType7=@bType7, wID7=@wID7, bType8=@bType8, wID8=@wID8
					, bType9=@bType9, wID9=@wID9, bType10=@bType10, wID10=@wID10, bType11=@bType11, wID11=@wID11
					, bType12=@bType12, wID12=@wID12 WHERE dwCharID = @dwCharID AND bInvenID = @bInven
ELSE
	DELETE THOTKEYTABLE WHERE dwCharID=@dwCharID AND bInvenID = @bInven


	SET @bLevel = 0
	SET @dwExp = 0
	SET @dwGold = 0
	SET @dwSilver = 0
	SET @dwCooper = 0
	SET @dCurDate = GetDate()

	IF(@dwCharID <> 0)
	BEGIN
		SELECT @bLevel = bLevel, @dwExp = dwEXP, @dwGold=dwGold, @dwSilver=dwSilver, @dwCooper=dwCooper FROM TCHARTABLE WHERE dwCharID = @dwCharID

		EXEC TUpdateActiveChar @dwCharID

		UPDATE TCHARTABLE SET dLogoutDate = @dCurDate WHERE dwCharID=@dwCharID

		EXEC @nResult = TGLOBAL_GSP.dbo.TLogout @dwUserID, @dwCharID, @bLevel, @dwExp
		IF(@nResult = 0)
		BEGIN
			SELECT @bWorldID = bWorld+1 FROM TDBITEMINDEXTABLE
			EXEC TGLOBAL_GSP.dbo.TUpdateCharMoney @bWorldID, @dwCharID, @dwGold, @dwSilver, @dwCooper
		END
	END

	EXEC TGLOBAL_GSP.DBO.TPcBangLogout @dwUserID, @dwPlayTime OUTPUT
/*
	IF(@dwGuildID <> 0 AND @dwPlayTime <> 0)
	BEGIN
		INSERT INTO TGUILDPLAYLOG(dwGuildID, dwUserID, dwCharID, dwPlayTime) VALUES(@dwGuildID, @dwUserID, @dwCharID, @dwPlayTime)
		UPDATE TGUILDTABLE SET dwPlayTime = dwPlayTime + @dwPlayTime  WHERE dwID = @dwGuildID
	END
*/
--	DELETE TITEMTABLE WHERE @dwCharID = dwOwnerID AND bOwnerType=0 AND wItemID = 7605

SELECT @dwUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwCharID
 
BEGIN TRAN SAVECHAREND
 
DELETE TITEMTABLE WHERE dwOwnerID = @dwCharID AND bOwnerType = 0 AND bStorageType <> 2
DELETE TITEMTABLE WHERE dlID IN(SELECT dlID FROM TTEMPITEMTABLE WHERE dwOwnerID = @dwCharID)

INSERT INTO TITEMTABLE(
        dlID, bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur,dEndTime,bGradeEffect,
        bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
        wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
        dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
        SELECT * FROM TTEMPITEMTABLE WHERE dwOwnerID = @dwCharID

DELETE TSKILLTABLE WHERE dwCharID = @dwCharID
INSERT INTO TSKILLTABLE SELECT * FROM TTEMPSKILLTABLE WHERE dwCharID = @dwCharID


 
COMMIT TRAN SAVECHAREND
 
EXEC TGLOBAL_GSP.DBO.TSaveDuringItem @dwUserID








	RETURN @nResult


