CREATE PROCEDURE [dbo].[TUpdateVersion]
@szPath varchar(260),
@szName varchar(260),
@dwSize int,
@dwBetaVer int
AS

DECLARE @dwVersion INT

IF EXISTS(SELECT TOP 1 dwVersion FROM TVERSION)
BEGIN
	SELECT @dwVersion = MAX(dwVersion) + 1 FROM TVERSION
END
ELSE
BEGIN
	SET @dwVersion = 1
END

IF EXISTS(SELECT TOP 1 dwVersion FROM TVERSION WHERE szPath = @szPath AND szName = @szName)
BEGIN
	UPDATE TVERSION SET dwVersion = @dwVersion, dwSize=@dwSize, dwBetaVer=@dwBetaVer WHERE szPath = @szPath AND szName = @szName
END
ELSE
BEGIN
	INSERT INTO TVERSION( dwVersion, szPath, szName, dwSize, dwBetaVer) VALUES( @dwVersion, @szPath, @szName, @dwSize, @dwBetaVer)
END

/*SET @dwVersion = (SELECT MAX(dwVersion) + 1 as dwVersion FROM TVERSION)

INSERT INTO TVERSION VALUES(1222, 'sss', 'ssss', 0, 0)*/

RETURN @@ERROR

