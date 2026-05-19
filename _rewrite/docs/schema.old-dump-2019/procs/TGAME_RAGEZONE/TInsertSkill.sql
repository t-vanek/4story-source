

CREATE PROCEDURE [dbo].[TInsertSkill] 
@dwCharID int,
@wSkillID smallint
AS

INSERT INTO TSKILLTABLE(dwCharID, wSkillID, bLevel, dwRemainTick) VALUES(@dwCharID, @wSkillID, 1,0)



