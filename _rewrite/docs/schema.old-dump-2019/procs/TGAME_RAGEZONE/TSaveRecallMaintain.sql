
CREATE PROCEDURE [dbo].[TSaveRecallMaintain]
@dwCharID int,
@wSkillID smallint,
@bLevel smallint,
@dwRemainTick int,
@bAttackType tinyint,
@dwAttackID int,
@bHostType tinyint,
@dwHostID int,
@bAttackCountry tinyint
AS
BEGIN TRAN TSKILLMAINTAIN
DELETE TSKILLMAINTAINTABLE WHERE dwCharID = @dwCharID
/*IF NOT EXISTS( SELECT dwCharID FROM TSKILLMAINTAINTABLE WHERE dwCharID = @dwCharID AND wSkillID = @wSkillID)
	INSERT INTO TSKILLMAINTAINTABLE(
		dwCharID,
		wSkillID,
		bLevel,
		dwRemainTick,
		bAttackType,
		dwAttackID,
		bHostType,
		dwHostID,
		bAttackCountry)  VALUES(
		@dwCharID,
		@wSkillID,
		@bLevel,
		@dwRemainTick,
		@bAttackType,
		@dwAttackID,
		@bHostType,
		@dwHostID,
		@bAttackCountry)*/	
COMMIT TRAN TSKILLMAINTAIN


