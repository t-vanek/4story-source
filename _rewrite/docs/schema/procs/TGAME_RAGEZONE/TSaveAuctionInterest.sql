




CREATE PROCEDURE [dbo].[TSaveAuctionInterest] 
	@dwCharID	INT,
	@dwAuctionID	INT
 AS
	IF(@dwAuctionID = 0 )
		DELETE TAUCTIONINTEREST WHERE dwCharID = @dwCharID
	ELSE
		INSERT INTO TAUCTIONINTEREST(dwCharID,dwAuctionID) VALUES(@dwCharID,@dwAuctionID)

	RETURN 0






