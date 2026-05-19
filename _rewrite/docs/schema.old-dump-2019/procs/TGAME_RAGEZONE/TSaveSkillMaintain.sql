CREATE PROCEDURE [dbo].[TSaveSkillMaintain]
@dwCharID int,
@wSkillID smallint,
@bLevel smallint,
@dwTick int,
@bAttackType tinyint,
@dwAttackID int,
@bHostType tinyint,
@dwHostID int,
@bAttackCountry tinyint
AS

BEGIN TRAN TEMPSKILLMAINTAIN
IF NOT EXISTS( SELECT dwCharID FROM TSKILLMAINTAINTABLE WHERE dwCharID = @dwCharID AND wSkillID = @wSkillID)
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
		@dwTick,
		@bAttackType,
		@dwAttackID,
		@bHostType,
		@dwHostID,
		@bAttackCountry)
COMMIT TRAN TEMPSKILLMAINTAIN


