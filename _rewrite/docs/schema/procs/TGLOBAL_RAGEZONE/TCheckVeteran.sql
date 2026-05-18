
CREATE PROCEDURE [dbo].[TCheckVeteran]
	@dwUserID		INT
AS
	DECLARE @bMaxLevel TINYINT
	DECLARE @bType TINYINT

	SET @bType = 0

	SELECT @bMaxLevel = MAX(bLevel) FROM TALLCHARTABLE WHERE dwUserID = @dwUserID

	SELECT @bType = bID FROM TVETERANCHART WHERE @bMaxLevel >= bLevel

	RETURN @bType



