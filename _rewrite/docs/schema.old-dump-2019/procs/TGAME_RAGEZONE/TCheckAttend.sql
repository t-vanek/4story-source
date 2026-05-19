


CREATE PROCEDURE [dbo].[TCheckAttend]
@dwUserID INT,
@dwCharID INT
AS

DECLARE @szName VARCHAR(50)
DECLARE @szMessage VARCHAR(500)
DECLARE @bGiveItem TINYINT

SET @szMessage = ''
SET @bGiveItem = 0

SELECT @szName = szName FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0)
	RETURN 1

EXEC TGLOBAL_GSP.DBO.TCheckAttend @dwUserID, @dwCharID, @szMessage OUTPUT, @bGiveItem OUTPUT

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
	'운영자',
	0,
	@szName,
	'출석체크',
	@szMessage,
	1,
	0,
	0,
	0,
	0,
	GetDate())

IF(@bGiveItem = 1)
BEGIN
	EXEC TEventItemGive @szName, 8224, 1, '3일 연속 출석 이벤트 당첨', '4스토리에 대한 많은 관심과 성원에 감사드립니다.'
END



