



/* FIND SERVER ID PROCESS

========================================================
PARAMETER
========================================================
@dwCharID		INT
@bChannel		TINYINT
@bServerID		TINYINT	OUTPUT

========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: NO SERVER

========================================================
PROCESS
========================================================
1. Find Map ID and Unit ID
2. Find Server ID

*/


CREATE PROCEDURE [dbo].[TFindServerID]
	@dwCharID		INT,
	@bChannel		TINYINT,
	@bServerID		TINYINT	OUTPUT
AS
	DECLARE @dwUserID	INT
	DECLARE @wMapID	SMALLINT
	DECLARE @wUnitID	SMALLINT
	DECLARE @wSpawn	SMALLINT
	DECLARE @fPosX	REAL
	DECLARE @fPosZ	REAL

	SET @bServerID = 0xFF

	SELECT TOP 1 @dwUserID = dwUserID, @wMapID = wMapID, @wSpawn = wSpawnID, @wUnitID = CAST(fPosZ / 1024 AS SMALLINT) * 256 + CAST(fPosX / 1024 AS SMALLINT) FROM TCHARTABLE WHERE dwCharID = @dwCharID
	IF @@ROWCOUNT = 0
	BEGIN
		/* Not found character */
		RETURN 1
	END

	SELECT TOP 1 @bServerID = bServerID FROM TSVRCHART INNER JOIN TCHANNELCHART ON
		TSVRCHART.bGroup = TCHANNELCHART.bGroupID AND
		TSVRCHART.wMapID = TCHANNELCHART.wMapID AND
		TSVRCHART.wUnitID = TCHANNELCHART.wUnitID AND
		TSVRCHART.bChannel = TCHANNELCHART.bPhyChannel
	WHERE TSVRCHART.wUnitID = @wUnitID AND TSVRCHART.wMapID = @wMapID AND TCHANNELCHART.bLogChannel = @bChannel

	IF @@ROWCOUNT = 0
	BEGIN
		SELECT @wMapID = wMapID, @fPosX=fPosX, @fPosZ=fPosZ FROM TSPAWNPOSCHART WHERE wID = @wSpawn
		IF @@ROWCOUNT = 0
		BEGIN
			/* Not found character */
			RETURN 2
		END

		SET @wUnitID = CAST(@fPosZ / 1024 AS SMALLINT) * 256 + CAST(@fPosX / 1024 AS SMALLINT)

		SELECT TOP 1 @bServerID = bServerID FROM TSVRCHART INNER JOIN TCHANNELCHART ON
			TSVRCHART.bGroup = TCHANNELCHART.bGroupID AND
			TSVRCHART.wMapID = TCHANNELCHART.wMapID AND
			TSVRCHART.wUnitID = TCHANNELCHART.wUnitID AND
			TSVRCHART.bChannel = TCHANNELCHART.bPhyChannel
		WHERE TSVRCHART.wUnitID = @wUnitID AND TSVRCHART.wMapID = @wMapID AND TCHANNELCHART.bLogChannel = @bChannel
	
		IF @@ROWCOUNT = 0
		BEGIN
			/* Not found server */
			RETURN 2
		END

		UPDATE TCHARTABLE SET wMapID = @wMapID, fPosX=@fPosX, fPosZ=@fPosZ, fPosY = 0 WHERE dwCharID = @dwCharID
	END

	EXEC TUpdateActiveChar @dwCharID
	EXEC TGLOBAL_GSP.DBO.TUpdateEnterLuckyDate @dwUserID
	EXEC TScammingPost @dwCharID
--	EXEC TPcBangItemGive @dwCharID
--	EXEC TCheckAttend @dwUserID, @dwCharID
--	EXEC TChristmasEventPost @dwCharID
--	EXEC TNewYearEventPost @dwCharID
--	EXEC TValentineEventPost @dwCharID
--	EXEC TWhitedayEventPost @dwCharID
--	EXEC TSeventhdayEventPost @dwCharID
--	EXEC THarvestFestivalEventPost @dwCharID
--	EXEC THalloweenEventPost @dwCharID
--	EXEC TOlympicEventPost @dwCharID
	EXEC TChangedPetSystemToMountSystem @dwUserID
	RETURN 0

