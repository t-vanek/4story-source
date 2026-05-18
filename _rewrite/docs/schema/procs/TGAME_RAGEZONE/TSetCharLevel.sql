
CREATE PROCEDURE [dbo].[TSetCharLevel]
@szName VARCHAR(50),
@bLevel TINYINT
AS

BEGIN
	UPDATE TCHARTABLE SET bLevel = @bLevel WHERE szName = @szName
END



