



CREATE PROCEDURE [dbo].[TSaveTax]
	@bType	TINYINT,
	@bCountry	TINYINT,
	@wNpcID	SMALLINT,
	@dwAddMoney	INT
 AS
	DECLARE	@dwMoney	INT
	IF(@dwAddMoney < 0 )
		SET @dwAddMoney = @dwAddMoney * (-1)

	SELECT @dwMoney = dwMoney FROM TTAXTABLE WHERE bCountry = @bCountry AND wNpcID = @wNpcID
	IF(@@ROWCOUNT = 0 )
	BEGIN
		IF( @dwAddMoney < 0 )
			SET @dwAddMoney = 0

		INSERT INTO TTAXTABLE ( bCountry, wNpcID, dwMoney ) VALUES (@bCountry, @wNpcID, @dwAddMoney)
	END
	ELSE
	BEGIN
		SET  @dwMoney = @dwMoney + @dwAddMoney
		IF ( @dwMoney < 0 )
			SET @dwMoney = 0
		
		UPDATE TTAXTABLE SET dwMoney = @dwMoney WHERE bCountry = @bCountry AND wNpcID = @wNpcID
	END






