
CREATE PROCEDURE [dbo].[TTESTGetMaxMagicValue]
@wItemID SMALLINT,
@bMagic TINYINT,
@wValue SMALLINT OUTPUT
 AS

declare @Value INT
declare @bRvType TINYINT
declare @fRev FLOAT
declare @wMaxValue SMALLINT
declare @dwKind INT
declare @bItemKind TINYINT

SET @wValue = 0

IF( @wItemID =0 OR @bMagic = 0)
	RETURN 1

IF(50 = @wItemID) SET @wItemID = 7210

select top 1 @wMaxValue = wMaxValue, @dwKind = dwKind, @bRvType = bRvType  from TITEMMAGICCHART where bmagic = @bMagic
IF(@@ROWCOUNT = 0)
	RETURN 1

IF(@bRvType = 0)
	select top 1 @fRev= 1, @bItemKind = bKind from titemchart where witemid = @wItemID
ELSE IF(@bRvType = 1)
	select top 1 @fRev= fRevision, @bItemKind = bKind from titemchart where witemid = @wItemID
ELSE IF(@bRvType = 2)
	select top 1 @fRev= fMRevision, @bItemKind = bKind from titemchart where witemid = @wItemID
ELSE IF(@bRvType = 3)
	select top 1 @fRev= fAtRate, @bItemKind = bKind from titemchart where witemid = @wItemID
ELSE IF(@bRvType = 4)
	select top 1 @fRev= fMAtRate, @bItemKind = bKind from titemchart where witemid = @wItemID
IF(@@ROWCOUNT = 0)
	RETURN 1

if not(POWER(2,@bItemKind-1) & @dwKind <> 0)
	return 1

SET @Value =( 65535 / @wMaxValue ) /@fRev
IF(32767 < @Value)
	SET @Value = 32767
IF (@Value IS NULL)
	SET @Value = 0

SET @wValue = @Value
return 0


