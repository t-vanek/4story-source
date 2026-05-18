
CREATE PROCEDURE [dbo].[TGuildWantedAdd]
@dwGuildID INT,
@bMinLevel TINYINT,
@bMaxLevel TINYINT,
@dEndTime SMALLDATETIME,
@szTitle VARCHAR(256),
@szText VARCHAR(2048)
AS

IF EXISTS(SELECT dwGuildID FROM TGUILDWANTEDTABLE WHERE dwGuildID=@dwGuildID)
	UPDATE TGUILDWANTEDTABLE SET bMinLevel=@bMinLevel, bMaxLevel=@bMaxLevel, dEndTime=@dEndTime,szTitle=@szTitle, szText=@szText WHERE dwGuildID=@dwGuildID
ELSE
	INSERT INTO TGUILDWANTEDTABLE (dwGuildID, bMinLevel, bMaxLevel, dEndTime, szTitle, szText) VALUES(@dwGuildID, @bMinLevel, @bMaxLevel, @dEndTime, @szTitle, @szText)


