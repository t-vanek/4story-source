

CREATE PROCEDURE [dbo].[TCreateRecallMon]
@dwMonID int output,
@dwCharID int,
@wMonTemp smallint,
@wPetID smallint,
@dwATTR int,
@bLevel tinyint,
@dwHP int,
@dwMP int,
@bSkillLevel tinyint,
@wPosX smallint,
@wPosY smallint,
@wPosZ smallint,
@dwTime int
AS

BEGIN TRAN CREATERECALLMON

INSERT INTO TRECALLMONTABLE(dwOwnerID, wMonID, wPetID, dwATTR, bLevel, dwHP, dwMP, bSkillLevel, wPosX, wPosY, wPosZ, dwTime)
	VALUES(@dwCharID, @wMonTemp, @wPetID, @dwATTR,  @bLevel, @dwHP, @dwMP, @bSkillLevel, @wPosX, @wPosY, @wPosZ, @dwTime)
SET @dwMonID = @@IDENTITY

COMMIT TRAN CREATERECALLMON


