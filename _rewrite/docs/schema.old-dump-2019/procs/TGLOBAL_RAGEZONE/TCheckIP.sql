CREATE PROCEDURE [dbo].[TCheckIP]
@szIPAddress varchar(50)
AS

-- 차단 예외 아이피
/*SELECT TOP 1 bAuthority FROM TIPAUTHORITY  WHERE @szIPAddress LIKE(szIP) AND bAuthority = 1 ORDER BY szIP DESC
IF(@@ROWCOUNT = 1)
	RETURN 1*/

-- 차단 아이피
SELECT TOP 1 bAuthority FROM TIPAUTHORITY WHERE @szIPAddress LIKE(szIP)
IF(@@ROWCOUNT = 1) 
	RETURN 6

/*RETURN 0*/

