CREATE PROCEDURE [dbo].[TSaveCompanion]
@dwCharID INT,
@bSlot TINYINT,
@dwMonID INT,
@bLevel TINYINT,
@strName VARCHAR(55),
@dwExp INT,
@wLife SMALLINT,
@bStatusPoints TINYINT,
@bEffect TINYINT,
@bSTR TINYINT,
@bDEX TINYINT,
@bCON TINYINT,
@bINT TINYINT,
@bWIS TINYINT,
@bMEN TINYINT,
@bBonusID TINYINT,
@wItemID0 SMALLINT,
@wItemID1 SMALLINT,
@dEndTime0 SMALLDATETIME,
@dEndTime1 SMALLDATETIME
AS
BEGIN
IF NOT EXISTS(SELECT dwMonID FROM TCOMPANIONTABLE WHERE bSlot = @bSlot and dwCharID = @dwCharID)
BEGIN
INSERT INTO TCOMPANIONTABLE
			(bSlot, dwMonID, bLevel, strName, dwExp, wLife, bStatusPoints, bEffect, wSTR, wDEX, wCON, wINT, wWIS, wMEN, wBonusID, dwCharID)
VALUES(@bSlot, @dwMonID, @bLevel, @strName, @dwExp, @wLife, @bStatusPoints, @bEffect, @bSTR, @bDEX, @bCON, @bINT, @bWIS, @bMEN, @bBonusID, @dwCharID)
END
ELSE
BEGIN
UPDATE TCOMPANIONTABLE SET bSlot = @bSlot, dwMonID = @dwMonID, bLevel = @bLevel, strName = @strName, dwExp = @dwExp, wLife = @wLife, bStatusPoints = @bStatusPoints, 
bEffect = @bEffect, wSTR = @bSTR, wDEX = @bDEX, wCON = @bCON, wINT = @bINT, wWIS = @bWIS, wMEN = @bMEN, wBonusID = @bBonusID WHERE dwCharID = @dwCharID and bSlot = @bSlot
END

IF NOT EXISTS(SELECT * FROM TCOMPANIONITEMTABLE WHERE dwCharID = @dwCharID and bSlot = @bSlot)
BEGIN
INSERT INTO TCOMPANIONITEMTABLE(bSlot, wFirstItemID, wSecondItemID, dFirstEndTime, dSecondEndTime, dwTick, dwCharID) 
												VALUES(@bSlot, @wItemID0, @wItemID1, @dEndTime0, @dEndTime1, 0, @dwCharID)
END
ELSE
BEGIN
UPDATE TCOMPANIONITEMTABLE SET wFirstItemID = @wItemID0, wSecondItemID = @wItemID1, dFirstEndTime = @dEndTime0, dSecondEndTime = @dEndTime1 WHERE dwCharID = @dwCharID and bSlot = @bSlot
END
END
