
CREATE PROCEDURE [dbo].[TSaveCharPosition]
@dwCharID INT,
@wMapID SMALLINT,
@fPosX REAL,
@fPosY REAL,
@fPosZ REAL,
@wDir SMALLINT
AS

UPDATE TCHARTABLE SET wMapID=@wMapID, fPosX=@fPosX, fPosY=@fPosY, fPosZ=@fPosZ, wDIR=@wDir WHERE dwCharID=@dwCharID


