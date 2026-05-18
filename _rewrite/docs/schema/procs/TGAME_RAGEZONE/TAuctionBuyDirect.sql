

/*
=========================================
RETURN VALUE
=========================================
3	: Invalid AuctionID
4	: Same Account
7	: Invalid Item Count
9	: Invalid Char
*/

CREATE PROCEDURE [dbo].[TAuctionBuyDirect]	
	@wNpcID	SMALLINT,
	@dwAuctionID	INT,
	@dwCharID	INT,	
	@bBuyCount	TINYINT,
	@dlBidPrice	BIGINT,	
	@dwSeller	INT		OUTPUT,
	@szSeller	VARCHAR(50)	OUTPUT,
	@dwDropBidder	INT		OUTPUT,
	@szDropBidder	VARCHAR(50)	OUTPUT,	
	@dlDropBidPrice BIGINT	OUTPUT	
 AS
	DECLARE	@bCount	TINYINT
	DECLARE	@dlID		BIGINT
	DECLARE 	@bCountry 	TINYINT
	DECLARE	@dwSellerUserID	INT
	DECLARE	@dwBidderUserID	INT

	SET @szDropBidder =''
	SET @dwDropBidder = 0
	SET @dlDropBidPrice = 0
	

	SELECT @dlID =  dlItemID  FROM TAUCTIONTABLE WHERE dwAuctionID = @dwAuctionID
	IF(@@ROWCOUNT = 0 )
		RETURN 3

	SELECT @bCountry = bCountry,@dwBidderUserID = dwUserID  FROM TCHARTABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT = 0 )
		RETURN 9
	 
	SELECT @dwSeller = T.dwCharID,@dwSellerUserID = T.dwUserID, @szSeller = T.szName  FROM TAUCTIONTABLE AS A INNER JOIN TCHARTABLE AS T ON A.dwCharID = T.dwCharID WHERE A.dwAuctionID = @dwAuctionID
	IF( @@ROWCOUNT = 0 )
		RETURN 9

	IF( @dwSellerUserID = @dwBidderUserID)
		RETURN 4

	IF EXISTS(SELECT dwCharID FROM TAUCTIONBIDDER WHERE dwAuctionID = @dwAuctionID  )
	BEGIN
		SELECT @szDropBidder  = C.szName,@dwDropBidder = A.dwCharID, @dlDropBidPrice = A.dlBidPrice FROM TAUCTIONBIDDER AS A INNER JOIN TCHARTABLE AS C ON A.dwCharID = C.dwCharID WHERE A.dwAuctionID =  @dwAuctionID

		DELETE TAUCTIONBIDDER WHERE dwCharID = @dwDropBidder
	END

	SELECT @bCount = bCount FROM TITEMTABLE WHERE dlID = @dlID

	IF (@bBuyCount > @bCount )
		RETURN 7
	
	IF(@bBuyCount = @bCount)
		DELETE TAUCTIONTABLE WHERE dwAuctionID = @dwAuctionID
	ELSE	
		UPDATE TITEMTABLE SET bCount = @bCount - @bBuyCount WHERE dlID = @dlID
	

	--세금징수
	DECLARE @dwTex	INT
	SET @dwTex = @dlBidPrice * @bBuyCount * 0.03
	EXEC TSaveTax 0,@bCountry,@wNpcID,@dwTex	

	RETURN 0



