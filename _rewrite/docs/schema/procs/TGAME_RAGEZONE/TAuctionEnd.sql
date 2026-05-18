




CREATE PROCEDURE [dbo].[TAuctionEnd]
	@wNpcID	SMALLINT,
	@dwAuctionID	INT,	
	@dwBuyer	INT		OUTPUT,
	@dwSeller	INT		OUTPUT,
	@szBuyer	VARCHAR(50)	OUTPUT,
	@szSeller	VARCHAR(50)	OUTPUT,
	@dlPrice	BIGINT		OUTPUT	
 AS
	DECLARE @bCountry	TINYINT
	DECLARE @DateEnd	SMALLDATETIME
	DECLARE @DateCur	SMALLDATETIME

	SELECT @dwSeller = dwCharID, @DateEnd = DateEnd  FROM TAUCTIONTABLE WHERE dwAuctionID = @dwAuctionID
	IF(@@ROWCOUNT = 0 )
		RETURN 1

	SELECT @szSeller = szName ,@bCountry = bCountry FROM TCHARTABLE WHERE dwCharID = @dwSeller
	
	SET @DateCur = GETDATE()
	IF(@DateCur < @DateEnd )
		RETURN 2

	SELECT @dwBuyer = A.dwCharID, @dlPrice = A.dlBidPrice , @szBuyer = T.szName FROM TAUCTIONBIDDER AS A INNER JOIN TCHARTABLE AS T ON A.dwCharID = T.dwCharID WHERE A.dwAuctionID = @dwAuctionID
	IF(@@ROWCOUNT = 0 )
		BEGIN
		SET @dwBuyer = 0
		SET @dlPrice = 0
		SET @szBuyer = ''
	END
	ELSE
	BEGIN
		--세금징수
		DECLARE @dwTex	INT
		SET @dwTex = @dlPrice * 0.03
		SET @dlPrice = @dlPrice - @dlPrice * 0.03
		EXEC TSaveTax 0,@bCountry,@wNpcID,@dwTex
		
	END
	
	-- Auction Delete
	DELETE TAUCTIONTABLE WHERE dwAuctionID = @dwAuctionID
	DELETE TAUCTIONBIDDER WHERE dwAuctionID = @dwAuctionID
	DELETE TAUCTIONINTEREST WHERE dwAuctionID = @dwAuctionID

	RETURN 0



