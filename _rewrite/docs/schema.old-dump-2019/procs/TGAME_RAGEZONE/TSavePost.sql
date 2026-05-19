

CREATE PROCEDURE [dbo].[TSavePost]
@dwMakeID INT OUTPUT,
@dwRecvID INT OUTPUT,
@dwSenderID INT,
@dwCharID INT,
@szTarget VARCHAR(50),
@szSender VARCHAR(50),
@szTitle VARCHAR(256),
@szMessage VARCHAR(2048),
@bRead TINYINT,
@bType TINYINT,
@dwGold INT,
@dwSilver INT,
@dwCooper INT,
@timeRecv SMALLDATETIME
AS

IF(@dwCharID = 0)
BEGIN
	SELECT @dwCharID = dwCharID FROM TCHARTABLE WHERE szName= @szTarget
	IF(@@ROWCOUNT = 0)
		RETURN 1
END

IF(@bType=1 AND @dwGold>10)
BEGIN
	INSERT INTO TPOSTERRORTABLE VALUES(@dwCharID, @dwGold)
END

SET @dwRecvID = @dwCharID

INSERT INTO TPOSTTABLE(
	dwCharID,
	szSender,
	dwSendID,
	szRecvName,
	szTitle,
	szMessage,
	bType,
	bRead,
	dwGold,
	dwSilver,
	dwCooper,
	timeRecv) VALUES(
	@dwCharID,
	@szSender,
	@dwSenderID,
	@szTarget,
	@szTitle,
	@szMessage,
	@bType,
	@bRead,
	@dwGold,
	@dwSilver,
	@dwCooper,
	@timeRecv)

SET @dwMakeID = @@IDENTITY



