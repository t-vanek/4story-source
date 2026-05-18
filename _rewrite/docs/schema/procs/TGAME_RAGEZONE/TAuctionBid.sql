



/*
==========================================
RETURN VALUE
==========================================
3	: Invalid AuctionID
5	: Low Bid Price
8	: Duplicate Bid
9	: Invalid Seller
*/

CREATE PROCEDURE [dbo].[TAuctionBid]	
	@wNpcID	SMALLINT,
	@dwAuctionID	INT,	
	@dwBidder	INT,	
	@dlBidPrice	BIGINT,
	@dlDropBidPrice BIGINT	OUTPUT,
	@dwDropBidder	INT 		OUTPUT,	
	@szDropBidder	VARCHAR(50)	OUTPUT,
	@dBidTime	SMALLDATETIME	OUTPUT
 AS
	DECLARE @bCountry		TINYINT
	DECLARE @dwSeller		INT
	DECLARE @dlItemID		BIGINT
	DECLARE @dLimitBidPrice	BIGINT
	DECLARE @fLowBidRate 	FLOAT
	DECLARE @bBidCount		TINYINT
	DECLARE @bRet		TINYINT
	DECLARE @dwDropBidderUserID	INT
	DECLARE @dwBidderUserID		INT
	DECLARE @dwSellerUserID		INT

	SET @fLowBidRate = 0.05

	SELECT @dwSeller = dwCharID,@bBidCount = bBidCount FROM TAUCTIONTABLE WHERE dwAuctionID = @dwAuctionID
	IF(@@ROWCOUNT = 0 )
		RETURN 3

	SELECT @dwSellerUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwSeller
	IF(@@ROWCOUNT = 0)
		RETURN 9

	SELECT @bCountry = bCountry, @dwBidderUserID = dwUserID FROM TCHARTABLE WHERE dwCharID = @dwBidder
	IF(@@ROWCOUNT = 0 )
		RETURN 9

	SELECT @dwDropBidderUserID = T.dwUserID ,@dwDropBidder = A.dwCharID, @dlDropBidPrice = A.dlBidPrice, @szDropBidder = T.szName  FROM TAUCTIONBIDDER AS A LEFT JOIN TCHARTABLE AS T ON A.dwCharID = T.dwCharID WHERE A.dwAuctionID = @dwAuctionID
	IF(@@ROWCOUNT = 0 )
	BEGIN
		SET @dwDropBidder = 0		
		SELECT @dlDropBidPrice = dlStartPrice FROM TAUCTIONTABLE WHERE dwAuctionID = @dwAuctionID
		SET @szDropBidder = ''
	END

	IF(@dwDropBidderUserID = @dwBidderUserID )
		RETURN 8

	IF( @dwSellerUserID = @dwBidderUserID )
		RETURN 4

	SET @dLimitBidPrice  = @dlDropBidPrice + @dlDropBidPrice * @fLowBidRate
	IF(@dlBidPrice < @dLimitBidPrice )
		RETURN 5

	SET @dBidTime = GETDATE()
	IF(@dwDropBidder = 0 )
		INSERT INTO TAUCTIONBIDDER(dwAuctionID,dwCharID,dlBidPrice,DateBid) VALUES (@dwAuctionID,@dwBidder,@dlBidPrice,@dBidTime)
	ELSE
		UPDATE TAUCTIONBIDDER SET dwCharID = @dwBidder, dlBidPrice = @dlBidPrice, DateBid = @dBidTime WHERE dwAuctionID = @dwAuctionID

	UPDATE TAUCTIONTABLE SET bBidCount = @bBidCount+1 WHERE dwAuctionID = @dwAuctionID
	
	RETURN 0



