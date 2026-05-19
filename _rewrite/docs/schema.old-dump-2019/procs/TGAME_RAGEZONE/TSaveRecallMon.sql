

CREATE PROCEDURE [dbo].[TSaveRecallMon]
@bDelete tinyint,
@dwCharID int,
@dwMonID int,
@wTempID smallint,
@wPetID smallint,
@dwATTR int,
@bLevel tinyint,
@dwHP int,
@dwMP int,
@bSkillLevel tinyint,
@wPosX smallint,
@wPosY smallint,
@wPosZ smallint,
@dwTick int,
@bEffect TINYINT
AS

IF(@bDelete = 1)
BEGIN
	DELETE TRECALLMONTABLE WHERE dwOwnerID=@dwCharID
	DELETE TRECALLMAINTAINTABLE WHERE dwCharID = @dwCharID
END
ELSE
	DELETE TRECALLMONTABLE WHERE dwID = @dwMonID
	INSERT INTO TRECALLMONTABLE(dwOwnerID, dwID, wMonID, wPetID, dwATTR, bLevel, dwHP, dwMP, bSkillLevel, wPosX, wPosY, wPosZ, dwTime, bEffect)
		VALUES(@dwCharID, @dwMonID, @wTempID, @wPetID, @dwATTR,  @bLevel, @dwHP, @dwMP, @bSkillLevel, @wPosX, @wPosY, @wPosZ, @dwTick, @bEffect)


