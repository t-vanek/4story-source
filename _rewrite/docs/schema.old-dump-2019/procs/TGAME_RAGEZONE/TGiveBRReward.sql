CREATE PROCEDURE [dbo].[TGiveBRReward]
@dwCharID INT,
@wKills SMALLINT,
@wItemID SMALLINT,
@bClass TINYINT,
@bRank TINYINT,
@bIsWinner TINYINT,
@wBonus SMALLINT
AS

DECLARE @bItemCount TINYINT
SET @bItemCount = @wKills + @wBonus

INSERT INTO TRESERVEDPOST(dwRecverID,szSender,szTitle,szMessage,bSend,wItemID,bLevel,bCount,bGLevel,dwDuraMax,dwDuraCur,
                                                    bRefineCur,    dEndTime,bMagic1,bMagic2,bMagic3,bMagic4,bMagic5,bMagic6,wValue1,wValue2,wValue3,wValue4,
                                                    wValue5,wValue6,dwTime1,dwTime2,dwTime3,dwTime4,dwTime5,dwTime6,bGem,wMoggItemID) 
                                    VALUES(    @dwCharID,'Battle Royal','Reward for Battle Royal','',0,@wItemID,0,@bItemCount,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
                                    
IF EXISTS(SELECT dwCharID FROM TRANKING WHERE dwCharID = @dwCharID)
BEGIN
    DECLARE @dwNewRankPoints INT
    DECLARE @dwCurrentRankPoints INT
    SET @dwCurrentRankPoints = (SELECT dwRankPoint FROM TRANKING WHERE dwCharID = @dwCharID)
    SET @dwNewRankPoints = @dwCurrentRankPoints + @bRank
    UPDATE TRANKING SET dwRankPoint = @dwNewRankPoints WHERE dwCharID = @dwCharID
END
ELSE
BEGIN
    INSERT INTO TRANKING (dwRankPoint, dwCharID) VALUES (@bRank, @dwCharID)
END
