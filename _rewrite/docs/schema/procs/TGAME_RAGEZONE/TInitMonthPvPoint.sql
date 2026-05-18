


/*
RETURN VALUE
0	:   누적 1위 변화없음
1	:   새로운 누적1위 설정
*/

CREATE PROCEDURE [dbo].[TInitMonthPvPoint]
	@bMonth		TINYINT,
	@dwMTotalPoint	INT,
	@dwCharID	INT 		OUTPUT,
	@szName	VARCHAR(50)	OUTPUT,
	@dwTotalPoint	INT		OUTPUT,
	@dwMonthPoint	INT		OUTPUT,
	@wMonthWin	SMALLINT	OUTPUT,
	@wMonthLose	SMALLINT	OUTPUT,
	@dwTotalWin	INT		OUTPUT,
	@dwTotalLose	INT		OUTPUT,
	@bCountry	TINYINT	OUTPUT,
	@bLevel	TINYINT	OUTPUT,
	@bClass	TINYINT	OUTPUT,
	@bRace	TINYINT	OUTPUT,
	@bSex		TINYINT	OUTPUT,
	@bHair		TINYINT	OUTPUT,
	@bFace	TINYINT	OUTPUT,
	@szGuild	VARCHAR(50)	OUTPUT
 AS
	TRUNCATE TABLE  tgame.TLASTTOTALPOINTTABLE
	TRUNCATE TABLE  tgame.TLASTMONTHPOINTTABLE

	INSERT INTO tgame.TLASTTOTALPOINTTABLE(dwCharID, dwPoint, dwWin, dwLose) 
	SELECT TOP 200 PT.dwCharID, PT.dwTotalPoint, PR.dwWarrior_win+PR.dwRanger_win+PR.dwArcher_win+PR.dwWizard_win+PR.dwPriest_win+PR.dwSorcerer_win, PR.dwWarrior_lose+PR.dwRanger_lose+PR.dwArcher_lose+PR.dwWizard_lose+PR.dwPriest_lose+PR.dwSorcerer_lose
	FROM TPVPOINTTABLE AS PT INNER JOIN TPVPRECORDTABLE AS PR ON PT.dwCharID=PR.dwCharID ORDER BY PT.dwTotalPoint DESC

	INSERT INTO tgame.TLASTMONTHPOINTTABLE(dwCharID, dwPoint, wWin, wLose) SELECT TOP 200 dwCharID, dwPoint, wWin, wLose FROM TMONTHPVPOINTTABLE  ORDER BY dwPoint DESC

	UPDATE TMONTHRANKTABLE SET dwTotalRank = LTP.dwRank FROM tgame.TLASTTOTALPOINTTABLE AS LTP INNER JOIN TMONTHRANKTABLE AS MR ON LTP.dwCharID=MR.dwCharID WHERE MR.bMonth=@bMonth

	DECLARE @dwGUildID	INT

	SELECT  @dwCharID = dwCharID, @dwTotalPoint = dwPoint FROM tgame.TLASTTOTALPOINTTABLE WHERE dwRank = 1
	IF(@@ROWCOUNT  = 0 )
	BEGIN
		UPDATE TMONTHPVPOINTTABLE SET dwPoint = 0, wWin = 0, wLose = 0,szSay=''
		RETURN 0
	END
		
/*
	IF(@dwTotalPoint <= @dwMTotalPoint )
	BEGIN
		UPDATE TMONTHPVPOINTTABLE SET dwPoint = 0, wWin = 0, wLose = 0,szSay=''
		RETURN 0
	END
*/	
 
	SELECT @szName = szName , @bCountry = bCountry, @bLevel = bLevel , @bClass = bClass, @bRace = bRace, @bSex = bSex, @bHair = bHair, @bFace = bFace FROM TCHARTABLE WHERE dwCharID = @dwCharID
	SELECT @dwGuildID = dwGuildID FROM TGUILDMEMBER WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT = 0 )
		SET @szGuild = ''
	ELSE
		SELECT @szGuild = szName FROM TGUILDTABLE WHERE dwID = @dwGuildID
	
	SELECT @wMonthWin = wWin, @wMonthLose = wLose FROM TMONTHPVPOINTTABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT = 0)
	BEGIN
		SET @wMonthWin = 0
		SET @wMonthLose = 0
	END

	SELECT @dwMonthPoint = dwPoint  FROM TMONTHPVPOINTTABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT = 0 )
		SET @dwMonthPoint = 0

	SELECT @dwTotalWin = dwWarrior_win + dwRanger_win + dwArcher_win + dwWizard_win + dwPriest_win + dwSorcerer_win , 
		@dwTotalLose = dwWarrior_lose + dwRanger_lose + dwArcher_lose + dwWizard_lose + dwPriest_lose + dwSorcerer_lose
	FROM TPVPRECORDTABLE WHERE dwCharID = @dwCharID

	INSERT INTO TMONTHRANKTABLE (
		bMonth,	bCountry,bRank,bMonthRank,dwTotalRank,dwCharID,szName,
		dwTotalPoint,dwMonthPoint,wMonthWin,wMonthLose,dwTotalWin,dwTotalLose,bLevel,bClass,bRace,bSex,bHair,bFace,szSay,szGuild) 
		 VALUES(
		@bMonth,@bCountry,0,0,1,@dwCharID,@szName,
		@dwTotalPoint,@dwMonthPoint,@wMonthWin,@wMonthLose,@dwTotalWin,@dwTotalLose,@bLevel,@bClass,@bRace,@bSex,@bHair,@bFace,'',@szGuild )

	EXEC TMonthRankReward @dwCharID, @szName, 0

	EXEC TMonthPvPointClear

	RETURN 1



