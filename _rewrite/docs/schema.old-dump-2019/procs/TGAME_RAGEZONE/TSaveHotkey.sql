

CREATE PROCEDURE [dbo].[TSaveHotkey]
@dwCharID INT,
@bSave TINYINT,
@bInven TINYINT,
@bType1 TINYINT,
@wID1 SMALLINT,
@bType2 TINYINT,
@wID2 SMALLINT,
@bType3 TINYINT,
@wID3 SMALLINT,
@bType4 TINYINT,
@wID4 SMALLINT,
@bType5 TINYINT,
@wID5 SMALLINT,
@bType6 TINYINT,
@wID6 SMALLINT,
@bType7 TINYINT,
@wID7 SMALLINT,
@bType8 TINYINT,
@wID8 SMALLINT,
@bType9 TINYINT,
@wID9 SMALLINT,
@bType10 TINYINT,
@wID10 SMALLINT,
@bType11 TINYINT,
@wID11 SMALLINT,
@bType12 TINYINT,
@wID12 SMALLINT
AS

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



