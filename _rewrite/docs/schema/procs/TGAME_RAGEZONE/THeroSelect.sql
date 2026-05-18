
/*
========================================================
RETURN VALUE
========================================================
0	: SUCCESS
2	: 존재하지 않는 캐릭터
6	: 전쟁시간
7	: 영웅이 이미 지정되었음

*/

CREATE PROCEDURE [dbo].[THeroSelect]
@dwCharID	  INT,
@dwGuildID   	  INT,
@wBattleZoneID SMALLINT,
@szHero 	  VARCHAR(50),
@dHeroTime 	  SMALLDATETIME
AS

DECLARE @dwCurGuild INT
DECLARE @dCurHero SMALLDATETIME
DECLARE @dDefendTime SMALLDATETIME


SELECT @dwCurGuild=dwGuild, @dCurHero=dateHero, @dDefendTime=dateDefend FROM TLOCALTABLE WHERE wLocalID = @wBattleZoneID
IF(@@ROWCOUNT > 0 AND @dwCurGuild=@dwGuildID)
BEGIN
	IF(@szHero <> '')
	BEGIN
		SELECT dwCharID FROM TCHARTABLE WHERE szName=@szHero
		IF(@@ROWCOUNT = 0 )
			RETURN 2

		SELECT dwCharID FROM TCHARTABLE_PW WHERE szName=@szHero AND bCountry = ( SELECT bCountry FROM TCHARTABLE_PW WHERE dwCharID = @dwCharID)
		IF(@@ROWCOUNT = 0 )
			RETURN 2

		IF(DATEDIFF(day, @dCurHero, @dDefendTime) < 1)
			RETURN 7

		IF(DATEDIFF(minute, @dHeroTime, @dDefendTime) < 30)
			RETURN 6

		UPDATE TLOCALTABLE SET szHero = @szHero, dateHero=GetDate() WHERE wLocalID = @wBattleZoneID
	END
	ELSE
		UPDATE TLOCALTABLE SET szHero = @szHero WHERE wLocalID = @wBattleZoneID

	RETURN 0
END

SELECT @dwCurGuild=dwGuildID, @dCurHero=dateHero, @dDefendTime=dateWarTime FROM TCASTLETABLE WHERE wCastle = @wBattleZoneID
IF(@@ROWCOUNT > 0 AND @dwCurGuild=@dwGuildID)
BEGIN	
	IF(@szHero <> '')
	BEGIN
		SELECT dwCharID FROM TCHARTABLE_PW WHERE szName=@szHero
		IF(@@ROWCOUNT = 0 )
			RETURN 2

		SELECT dwCharID FROM TGUILDMEMBERTABLE WHERE dwGuildID = @dwGuildID AND dwCharID = (SELECT dwCharID FROM TCHARTABLE_PW WHERE szName=@szHero)
		IF( @@ROWCOUNT = 0 )
			RETURN 2

		IF(DATEDIFF(day, @dCurHero, @dDefendTime) < 7)
			RETURN 7

		IF(DATEDIFF(minute, @dHeroTime, @dDefendTime) < 60)
			RETURN 6

		UPDATE TCASTLETABLE SET szHero = @szHero, dateHero=GetDate() WHERE wCastle = @wBattleZoneID
	END
	ELSE
		UPDATE TCASTLETABLE SET szHero = @szHero WHERE wCastle = @wBattleZoneID

	RETURN 0
END

RETURN 0


