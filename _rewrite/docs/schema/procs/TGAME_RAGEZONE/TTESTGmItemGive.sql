
CREATE PROCEDURE [dbo].[TTESTGmItemGive]
@szName VARCHAR(50)
AS

declare @dwCharid INT
declare @dwUserId INT
declare @bClass TINYINT
declare @wArmor SMALLINT
declare @bMaxLevel TINYINT
declare @dwExp INT
declare @GenID BIGINT

select @dwCharid = dwcharid, @dwUserId = dwUserID, @bClass = bclass from TCHARTABLE where szname = @szName
IF(@@ROWCOUNT <> 1)
  RETURN 'Not Find Character'

SET @bMaxLevel = 97
SELECT @dwExp = dwExp FROM TLEVELCHART WHERE bLevel = @bMaxLevel-1

UPDATE TCHARTABLE set wskillpoint = 0, blevel = @bMaxLevel, dwgold = 200, dwExp = @dwExp where  dwcharid = @dwCharid

DELETE tskilltable where dwcharid = @dwCharid
INSERT TSKILLTABLE (dwCharID, wSkillID, bLevel, dwRemainTick) SELECT @dwCharid, wID, bmaxlevel, 0 FROM TSKILLCHART WHERE (dwClassid & power(2,@bClass) <> 0) and bCanLearn =1

DELETE TITEMTABLE where dwOwnerID = @dwCharid
DELETE TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE WHERE dwUserID = @dwUserID
DELETE TPOSTTABLE where dwCharID = @dwCharid
DELETE THOTKEYTABLE WHERE dwCharID = @dwCharid

delete TINVENTABLE where dwCharID = @dwCharid and bInvenID in(0,1,2,3,4)
insert TINVENTABLE (dwCharID, bInvenID, wItemID, dEndTime, bELD) values(@dwCharid, 0, 12, 0, 0)
insert TINVENTABLE (dwCharID, bInvenID, wItemID, dEndTime, bELD) values(@dwCharid, 1, 12, 0, 0)
insert TINVENTABLE (dwCharID, bInvenID, wItemID, dEndTime, bELD) values(@dwCharid, 2, 12, 0, 0)
insert TINVENTABLE (dwCharID, bInvenID, wItemID, dEndTime, bELD) values(@dwCharid, 3, 12, 0, 0)
insert TINVENTABLE (dwCharID, bInvenID, wItemID, dEndTime, bELD) values(@dwCharid, 4, 12, 0, 0)

EXEC TEventItemGive @szName, 51, 1,'GM HIDE',''
EXEC TEventItemGive @szName, 53, 1,'GM SPEED',''
EXEC TEventItemGive @szName, 55, 1,'GM FLY',''
EXEC TEventItemGive @szName, 56, 1,'GM SOUL',''
EXEC TEventItemGive @szName, 57, 1,'GM COSTUME',''

--EXEC TTESTGivePowerItem @dwCharid, 50, 10, 0, 0
--EXEC TTESTGivePowerItem @dwCharid, 50, 11, 2, 0


--SET @Gen1 = EXEC TGenerateDBItemID 281474976779622

insert TITEMTABLE (dlID, bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,

	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6, 
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
  values(762216513855943,0,254,0,@dwCharid, 11,50,0,1,0,0,0,0,0,2,
	0,0,3,0,0,0, 
	0,0,5461,0,0,0, 
	0,0,0,0,0,0,0,0)

insert TITEMTABLE (dlID, bStorageType, dwStorageID, bOwnerType, dwOwnerID, bItemID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,

	bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6, 
	wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
	dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
  values(657162376684541,0,254,0,@dwCharid, 10,50,0,1,0,0,0,0,0,2,
	0,0,3,0,0,0, 
	0,0,5461,0,0,0, 
	0,0,0,0,0,0,0,0)

INSERT INTO TGLOBAL_GSP.DBO.TCASHITEMCABINETTABLE(
		dwUserID, wItemID, bLevel, bCount, bGLevel, dwDuraMax, dwDuraCur, bRefineCur, dEndTime, bGradeEffect,
		bMagic1, bMagic2, bMagic3, bMagic4, bMagic5, bMagic6,
		wValue1, wValue2, wValue3, wValue4, wValue5, wValue6,
		dwTime1, dwTime2, dwTime3, dwTime4, dwTime5, dwTime6, bGem, wMoggItemID)
	 	VALUES(@dwUserID, 16031, 20, 1, 0, 9999, 9999, 0, 0, 2,
		0, 0, 3, 0, 0, 0, 
		0, 0, 5461, 0, 0, 0, 
		0, 0, 0, 0, 0, 0, 0, 0)


	EXEC TTESTGivePowerItem @dwCharid, 23705, 0, 2, 20 --검
	SET @wArmor = 3126



-- 이펙트 타입 0:없음, 1:물, 2:불, 3:전기, 4:ICE, 5:암흑
EXEC TTESTGivePowerItem @dwCharid, @wArmor, 3, 2, 20
SET @wArmor = @wArmor +1
EXEC TTESTGivePowerItem @dwCharid, @wArmor, 5, 2, 20
SET @wArmor = @wArmor +1
EXEC TTESTGivePowerItem @dwCharid, @wArmor, 6, 2, 20
SET @wArmor = @wArmor +1
EXEC TTESTGivePowerItem @dwCharid, @wArmor, 7, 2, 20
SET @wArmor = @wArmor +1
EXEC TTESTGivePowerItem @dwCharid, @wArmor, 8, 2, 20
EXEC TTESTGivePowerItem @dwCharid, 7001, 4, 0, 20

/*EXEC TItemInsert @dwUserID, 7549, 1
EXEC TItemInsert @dwUserID, 7550, 1
EXEC TItemInsert @dwUserID, 7551, 1
EXEC TItemInsert @dwUserID, 7556, 1
EXEC TItemInsert @dwUserID, 7602, 200
EXEC TItemInsert @dwUserID, 7656, 200
EXEC TItemInsert @dwUserID, 7658, 200
EXEC TItemInsert @dwUserID, 7659, 200
EXEC TItemInsert @dwUserID, 7663, 200
EXEC TItemInsert @dwUserID, 8505, 200
EXEC TItemInsert @dwUserID, 18053, 200
EXEC TItemInsert @dwUserID, 6804, 200
EXEC TItemInsert @dwUserID, 6807, 200
EXEC TItemInsert @dwUserID, 6808, 200
EXEC TItemInsert @dwUserID, 7601, 200
EXEC TItemInsert @dwUserID, 7603, 200
EXEC TItemInsert @dwUserID, 7604, 200
EXEC TItemInsert @dwUserID, 7605, 200
EXEC TItemInsert @dwUserID, 7611, 200
EXEC TItemInsert @dwUserID, 7612, 200
EXEC TItemInsert @dwUserID, 7613, 200
EXEC TItemInsert @dwUserID, 7618, 200
EXEC TItemInsert @dwUserID, 7619, 200
EXEC TItemInsert @dwUserID, 7620, 200
EXEC TItemInsert @dwUserID, 7621, 200
EXEC TItemInsert @dwUserID, 7622, 200
EXEC TItemInsert @dwUserID, 7623, 200
EXEC TItemInsert @dwUserID, 7609, 200
EXEC TItemInsert @dwUserID, 7678, 200
EXEC TItemInsert @dwUserID, 7679, 200
EXEC TItemInsert @dwUserID, 18142, 200*/

