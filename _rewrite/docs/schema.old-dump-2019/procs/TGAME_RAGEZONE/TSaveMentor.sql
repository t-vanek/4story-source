
CREATE PROCEDURE [dbo].[TSaveMentor]
@dwCharID INT,
@dwMentor INT,
@dwExp INT,
@bDelete TINYINT
AS

IF(@bDelete > 0)
	DELETE TMENTORTABLE WHERE dwCharID = @dwCharID
ELSE
BEGIN
	IF EXISTS (SELECT dwCharID FROM TMENTORTABLE WHERE dwCharID = @dwCharID)
		UPDATE TMENTORTABLE SET dwExp = @dwExp WHERE dwCharID = @dwCharID
	ELSE
		INSERT INTO TMENTORTABLE(dwCharID, dwMentorID, dwExp) VALUES(@dwCharID, @dwMentor, @dwExp)
END


