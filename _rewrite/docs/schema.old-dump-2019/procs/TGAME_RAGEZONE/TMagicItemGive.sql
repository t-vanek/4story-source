
CREATE PROCEDURE [dbo].[TMagicItemGive]
	@szRecver		VARCHAR(50),
	@wItemID		SMALLINT,
	@szTitle		VARCHAR(256),
	@szMessage		VARCHAR(2048),	
	@bMagic1		TINYINT = 0,
	@wValue1		SMALLINT = 0,
	@bMagic2		TINYINT = 0,
	@wValue2		SMALLINT = 0,
	@bMagic3		TINYINT = 0,
	@wValue3		SMALLINT = 0,
	@bMagic4		TINYINT = 0,
	@wValue4		SMALLINT = 0,
	@bMagic5		TINYINT = 0,
	@wValue5		SMALLINT = 0,
	@bMagic6		TINYINT = 0,
	@wValue6		SMALLINT = 0
 AS

DECLARE 	@dwCharID	 INT
DECLARE	@bLenTitle	BINARY(4)
DECLARE	@bLenMessage BINARY(4)
DECLARE 	@szT 		VARCHAR(8)
DECLARE 	@szM		VARCHAR(8)
DECLARE	@szSender	VARCHAR(50)

SET @szSender ='운영자'

SELECT @dwCharID=dwCharID FROM TCHARTABLE WHERE szName=@szRecver AND bDelete = 0
IF @@ROWCOUNT = 0
BEGIN
	PRINT 'FAIL'
	RETURN 1
END

SET @bLenTitle = DATALENGTH(@szTitle)
SET @bLenMessage = DATALENGTH(@szMessage)
SET @szT = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenTitle), 8)
SET @szTitle  = @szT + @szTitle
SET @szM = RIGHT(master.dbo.fn_sqlvarbasetostr(@bLenMessage), 8)
SET @szMessage = @szM + @szMessage

DECLARE 	@bRvType	TINYINT
DECLARE 	@fRevision	FLOAT
DECLARE	@wMaxValue	SMALLINT
DECLARE	@wRetValue1	SMALLINT
DECLARE	@wRetValue2	SMALLINT
DECLARE	@wRetValue3	SMALLINT
DECLARE	@wRetValue4	SMALLINT
DECLARE	@wRetValue5	SMALLINT
DECLARE	@wRetValue6	SMALLINT


-- Magic1
IF(@bMagic1 = 0 )
	SET @wRetValue1 = 0
ELSE
BEGIN

	SELECT @bRvType = bRvType,@wMaxValue = wMaxValue  FROM TITEMMAGICCHART WHERE bMagic = @bMagic1
	IF @wValue1 > @wMaxValue
		SET @wValue1 = @wMaxValue

	SELECT @fRevision =  CASE  
				WHEN @bRvType = 1 THEN  fRevision 
				WHEN @bRvType =  2 THEN  fMRevision 
				WHEN @bRvType =  3 THEN  fAtRate
				WHEN @bRvType =  4 THEN  fMAtRate
				ELSE 1.0
	END				
	FROM TITEMCHART WHERE wItemID = @wItemID

	SET @wRetValue1 = @wValue1 * 100.0 / @wMaxValue/@fRevision + 1.0
END


-- Magic2
IF(@bMagic2 = 0 )
	SET @wRetValue2 = 0
ELSE
BEGIN
	
	SELECT @bRvType = bRvType , @wMaxValue = wMaxValue  FROM TITEMMAGICCHART WHERE bMagic = @bMagic2	
	IF @wValue2 > @wMaxValue
		SET @wValue2 = @wMaxValue

	SELECT @fRevision =  CASE  
				WHEN @bRvType = 1 THEN  fRevision 
				WHEN @bRvType =  2 THEN  fMRevision 
				WHEN @bRvType =  3 THEN  fAtRate
				WHEN @bRvType =  4 THEN  fMAtRate
				ELSE 1.0
	END				
	FROM TITEMCHART WHERE wItemID = @wItemID
	
	SET @wRetValue2 = @wValue2 * 100.0 / @wMaxValue/@fRevision + 1.0
END

-- Magic3
IF(@bMagic3 = 0 )
	SET @wRetValue3 = 0
ELSE
BEGIN
	SELECT @bRvType = bRvType,@wMaxValue = wMaxValue  FROM TITEMMAGICCHART WHERE bMagic = @bMagic3
	IF @wValue3 > @wMaxValue
		SET @wValue3 = @wMaxValue

	SELECT @fRevision =  CASE  
				WHEN @bRvType = 1 THEN  fRevision 
				WHEN @bRvType =  2 THEN  fMRevision 
				WHEN @bRvType =  3 THEN  fAtRate
				WHEN @bRvType =  4 THEN  fMAtRate
				ELSE 1.0
	END				
	FROM TITEMCHART WHERE wItemID = @wItemID

	SET @wRetValue3 = @wValue3 * 100.0 / @wMaxValue/@fRevision + 1.0
END


-- Magic4
IF(@bMagic4 = 0 )
	SET @wRetValue4 = 0
ELSE
BEGIN
	SELECT @bRvType = bRvType,@wMaxValue = wMaxValue  FROM TITEMMAGICCHART WHERE bMagic = @bMagic4
	IF @wValue4 > @wMaxValue
		SET @wValue4 = @wMaxValue

	SELECT @fRevision =  CASE  
				WHEN @bRvType = 1 THEN  fRevision 
				WHEN @bRvType =  2 THEN  fMRevision 
				WHEN @bRvType =  3 THEN  fAtRate
				WHEN @bRvType =  4 THEN  fMAtRate
				ELSE 1.0
	END				
	FROM TITEMCHART WHERE wItemID = @wItemID

	SET @wRetValue4 = @wValue4 * 100.0 / @wMaxValue/@fRevision + 1.0
END

-- Magic5
IF(@bMagic5 = 0 )
	SET @wRetValue5 = 0
ELSE
BEGIN
	SELECT @bRvType = bRvType,@wMaxValue = wMaxValue  FROM TITEMMAGICCHART WHERE bMagic = @bMagic5
	IF @wValue5 > @wMaxValue
		SET @wValue5 = @wMaxValue

	SELECT @fRevision =  CASE  
				WHEN @bRvType = 1 THEN  fRevision 
				WHEN @bRvType =  2 THEN  fMRevision 
				WHEN @bRvType =  3 THEN  fAtRate
				WHEN @bRvType =  4 THEN  fMAtRate
				ELSE 1.0
	END				
	FROM TITEMCHART WHERE wItemID = @wItemID

	SET @wRetValue5 = @wValue5 * 100.0 / @wMaxValue/@fRevision + 1.0
END


-- Magic6
IF(@bMagic6 = 0 )
	SET @wRetValue6 = 0
ELSE
BEGIN
	SELECT @bRvType = bRvType,@wMaxValue = wMaxValue  FROM TITEMMAGICCHART WHERE bMagic = @bMagic6
	IF @wValue6 > @wMaxValue
		SET @wValue6 = @wMaxValue

	SELECT @fRevision =  CASE  
				WHEN @bRvType = 1 THEN  fRevision 
				WHEN @bRvType =  2 THEN  fMRevision 
				WHEN @bRvType =  3 THEN  fAtRate
				WHEN @bRvType =  4 THEN  fMAtRate
				ELSE 1.0
	END				
	FROM TITEMCHART WHERE wItemID = @wItemID

	SET @wRetValue6 = @wValue6 * 100.0 / @wMaxValue/@fRevision + 1.0
END


DECLARE @dwDuraMax INT
SET @dwDuraMax = 0
SELECT @dwDuraMax = dwDuraMax FROM TITEMCHART WHERE wItemID = @wItemID

INSERT INTO TRESERVEDPOST(
	dwRecverID,
	szSender,
	szTitle,
	szMessage,
	bSend,
	wItemID,
	bLevel,
	bCount,
	bGLevel,
	dwDuraMax,
	dwDuraCur,
	bRefineCur,
	dEndTime,
	bMagic1,
	bMagic2,
	bMagic3,
	bMagic4,
	bMagic5,
	bMagic6,
	wValue1,
	wValue2,
	wValue3,
	wValue4,
	wValue5,
	wValue6,
	dwTime1,
	dwTime2,
	dwTime3,
	dwTime4,
	dwTime5,
	dwTime6,
	bGem) VALUES(
	@dwCharID,
	@szSender,
	@szTitle,
	@szMessage,
	0,
	@wItemID,
	0,
	1,
	0,
	@dwDuraMax,
	@dwDuraMax,
	0,
	0,
	@bMagic1,@bMagic2,@bMagic3,@bMagic4,@bMagic5,@bMagic6,
	@wRetValue1,@wRetValue2,@wRetValue3,@wRetValue4,@wRetValue5,@wRetValue6,
	0,0,0,0,0,0,0)

	RETURN 0


