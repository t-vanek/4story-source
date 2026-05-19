


/*
--------------------------------------------------------------------
RETURN VALUE
--------------------------------------------------------------------
1	:	INVALID CHARACTER

*/

CREATE PROCEDURE [dbo].[TGetMonthPvPoint] 
	@dwCharID		INT,
	@dwMonthPoint		INT		OUTPUT,
	@wMonthWin		SMALLINT	OUTPUT,
	@wMonthLose		SMALLINT	OUTPUT,
	@dwMonthRankOrder	INT		OUTPUT,
	@bMonthRankPercent	TINYINT	OUTPUT
 AS
	SET @dwMonthPoint  = 0
	SET @wMonthWin = 0
	SET @wMonthLose = 0
	SET @dwMonthRankOrder = 0
	SET @bMonthRankPercent = 0

	SELECT @dwMonthPoint = dwPoint, @wMonthWin = wWin, @wMonthLose = wLose FROM TMONTHPVPOINTTABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT = 0 OR @dwMonthPoint = 0)
		RETURN 0

	SELECT @dwMonthRankOrder = COUNT(dwCharID)+1 FROM TMONTHPVPOINTTABLE WHERE dwPoint > @dwMonthPoint 
	SELECT @bMonthRankPercent = @dwMonthRankOrder / COUNT(dwCharID) * 100  FROM TCHARTABLE
	IF(@bMonthRankPercent = 0)
		SET @bMonthRankPercent = 1

	RETURN 0




