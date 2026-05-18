CREATE PROCEDURE [dbo].[TUpdateEnterLuckyDate]
@dwUserID INT
AS

UPDATE TCURRENTUSER SET dEnterDate = GetDate(), bLuckyNumber = CAST(RAND()*100 AS TINYINT)  WHERE dwUserID = @dwUserID
--UPDATE TCURRENTUSER SET dEnterDate = GetDate(), bLuckyNumber = CAST(RAND()*1 AS TINYINT)  WHERE dwUserID = @dwUserID

