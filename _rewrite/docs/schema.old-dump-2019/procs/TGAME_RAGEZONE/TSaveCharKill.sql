

CREATE PROCEDURE [dbo].[TSaveCharKill]
@dwKillerID INT,
@dwTargetID INT
AS


INSERT INTO charkilling_log(dwKillerID, dwTargetID) VALUES(@dwKillerID, @dwTargetID)
