




/*
========================================================
RETURN VALUE
========================================================
0	: SUCCESS
3	: NO USER
4	: DirectPrice < StartPrice
6	: ITEM NOTFOUND
*/

CREATE PROCEDURE [dbo].[TAuctionReg]
	@wNpcID	SMALLINT,
	@wHour	SMALLINT,
	@dwSeller	INT,	
	@dlDirectPrice	BIGINT,
	@dlStartPrice	BIGINT,	
	@dlID		BIGINT,
	@dwAuctionID	INT			OUTPUT,
	@DateStart	SMALLDATETIME 	OUTPUT,
	@DateEnd	SMALLDATETIME	OUTPUT
 AS
	DECLARE @szSeller	VARCHAR(50)
	DECLARe @bCountry	TINYINT

	SELECT @szSeller = szName, @bCountry = bCountry FROM TCHARTABLE WHERE dwCharID = @dwSeller
	IF(@@ROWCOUNT = 0 )
		RETURN 3

	IF( @dlDirectPrice < @dlStartPrice )
		RETURN 4

	SELECT wItemID FROM TITEMTABLE WHERE dlID = @dlID
	IF(@@ROWCOUNT = 0 )
		RETURN 6

	SET @DateStart = GETDATE();
	SET @DateEnd = DateAdd(HOUR,@wHour,@DateStart)

	UPDATE TITEMTABLE SET bOwnerType = 2, dwOwnerID = @wNpcID WHERE dlID = @dlID

	INSERT INTO TAUCTIONTABLE(
		wNpcID,
		dwCharID,
		DateStart,
		DateEnd,
		dlDirectPrice,
		dlStartPrice,
		dlItemID,
		bBidCount) VALUES(
		@wNpcID,
		@dwSeller,
		@DateStart,
		@DateEnd,
		@dlDirectPrice,
		@dlStartPrice,
		@dlID,
		0)

	SET @dwAuctionID = @@IDENTITY
	
	--세금징수
	DECLARE @dwTex	INT
	SET @dwTex = @dlDirectPrice * 0.03
	EXEC TSaveTax 0,@bCountry,@wNpcID,@dwTex

	RETURN 0



