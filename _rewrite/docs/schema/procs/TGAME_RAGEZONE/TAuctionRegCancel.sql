




CREATE PROCEDURE [dbo].[TAuctionRegCancel]
	@wNpcID	SMALLINT,
	@dwAuctionID	INT,
	@szSeller	VARCHAR(50)	OUTPUT,
	@szBidder	VARCHAR(50)	OUTPUT,
	@dwBidder	INT 		OUTPUT,
	@dlBidPrice	BIGINT		OUTPUT	
 AS
	DECLARE	@dlID		BIGINT

	SELECT @szSeller = T.szName  FROM TAUCTIONTABLE AS A INNER JOIN TCHARTABLE AS T ON A.dwCharID = T.dwCharID WHERE A.dwAuctionID = @dwAuctionID
	IF( @@ROWCOUNT = 0)
		RETURN 3
	
	SELECT @dlID = dlItemID FROM TAUCTIONTABLE WHERE dwAuctionID = @dwAuctionID
	SELECT wItemID FROM TITEMTABLE WHERE dlID = @dlID
	IF(@@ROWCOUNT = 0)
		RETURN 6

	SELECT @dwBidder = A.dwCharID, @dlBidPrice = A.dlBidPrice  , @szBidder = T.szName FROM TAUCTIONBIDDER AS A LEFT JOIN TCHARTABLE AS T ON A.dwCharID = T.dwCharID WHERE A.dwAuctionID = @dwAuctionID
	IF ( @@ROWCOUNT = 0 )
	BEGIN
		SET @dwBidder = 0
		SET @dlBidPrice = 0
		SET @szBidder =''
	END


	BEGIN TRAN REGCANCEL
	
	DELETE TAUCTIONTABLE WHERE dwAuctionID = @dwAuctionID AND wNpcID = @wNpcID
	DELETE TAUCTIONINTEREST WHERE dwAuctionID = @dwAuctionID
	DELETE TAUCTIONBIDDER WHERE dwAuctionID = @dwAuctionID
	
	COMMIT TRAN REGCANCEL

	RETURN 0



