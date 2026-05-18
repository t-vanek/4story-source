
CREATE PROCEDURE [dbo].[TGuildTacticsWantedAdd]
@dwID INT,
@dwGuildID INT,
@dwPoint INT,
@dwGold INT,
@dwSilver INT,
@dwCooper INT,
@bDay TINYINT,
@bMinLevel TINYINT,
@bMaxLevel TINYINT,
@dEndTime SMALLDATETIME,
@szTitle VARCHAR(256),
@szText VARCHAR(2048)
AS

IF EXISTS(SELECT dwID FROM TGUILDTACTICSWANTEDTABLE WHERE dwID=@dwID)
	UPDATE TGUILDTACTICSWANTEDTABLE SET szTitle=@szTitle, szText=@szText WHERE dwID=@dwID
ELSE
	INSERT INTO TGUILDTACTICSWANTEDTABLE	(
		dwID, dwGuildID, dwPvPoint, dwGold, dwSilver, dwCooper, bDay, bMinLevel, bMaxLevel, szTitle, szText, dEndTime) VALUES(
		@dwID, @dwGuildID, @dwPoint, @dwGold, @dwSilver, @dwCooper, @bDay, @bMinLevel, @bMaxLevel, @szTitle, @szText, @dEndTime)


