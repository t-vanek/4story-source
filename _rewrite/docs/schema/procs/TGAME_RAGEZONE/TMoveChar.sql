

CREATE PROCEDURE [dbo].[TMoveChar]
@szName varchar(50),
@bCountry tinyint
AS

IF(@bCountry=0)
	UPDATE TCHARTABLE SET wMapID=0,fPosX=2873, fPosY=65, fPosZ=5035 where szName=@szName
ELSE IF(@bCountry=1)
	UPDATE TCHARTABLE SET wMapID=0,fPosX=5398, fPosY=85, fPosZ=4625 where szName=@szName



