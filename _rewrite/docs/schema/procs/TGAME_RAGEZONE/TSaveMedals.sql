CREATE PROCEDURE [dbo].[TSaveMedals]
@dwCharID INT,
@dwMedals INT
AS

IF EXISTS ( SELECT dwMedals FROM TMEDALS WHERE dwCharID = @dwCharID)
BEGIN
	UPDATE TMEDALS SET dwMedals = @dwMedals WHERE dwCharID = @dwCharID
END
ELSE
BEGIN
	INSERT INTO TMEDALS (dwCharID, dwMedals) VALUES (@dwCharID, @dwMedals)
END
