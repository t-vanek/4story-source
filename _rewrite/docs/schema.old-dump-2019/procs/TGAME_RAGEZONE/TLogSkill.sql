

CREATE PROCEDURE [dbo].[TLogSkill]
	@dwCharID	INT,
	@dwGold	INT,
	@dwSilver	INT,
	@dwCooper	INT,
	@wSkill		SMALLINT,
	@bLevel	TINYINT
AS

INSERT INTO TSKILLLOG(
	dwCharID,
	dwGold,
	dwSilver,
	dwCooper,
	wSkill,
	bLevel,
	timeInsert) VALUES(
	@dwCharID,
	@dwGold,
	@dwSilver,
	@dwCooper,
	@wSkill,
	@bLevel,
	getdate())



