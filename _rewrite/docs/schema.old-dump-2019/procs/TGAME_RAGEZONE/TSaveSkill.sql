CREATE PROCEDURE [dbo].[TSaveSkill]
@dwCharID int,
@wSkill smallint,
@bLevel tinyint,
@dwTick int
AS

BEGIN TRAN SAVESKILL

	INSERT INTO TTEMPSKILLTABLE(dwCharID, wSkillID, bLevel, dwRemainTick) VALUES(@dwCharID, @wSkill, @bLevel, @dwTick)

COMMIT TRAN SAVESKILL



