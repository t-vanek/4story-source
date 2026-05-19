

CREATE PROCEDURE [dbo].[TEventUpdate] 
	@dwIndex	INT,
	@bID		TINYINT,
	@bType	TINYINT,
	@szTitle	VARCHAR(50),
	@bGroupID	TINYINT,
	@bSvrType	TINYINT,
	@bSvrID	TINYINT,
	@dStartDate	SMALLDATETIME,
	@dEndDate	SMALLDATETIME,
	@wValue	SMALLINT,
	@wMapID	SMALLINT,
	@dwStartAlarm	INT,
	@dwEndAlarm	INT,
	@bPartTime	TINYINT,
	@szStartMsg	VARCHAR(1024),
	@szMidMsg	VARCHAR(1024),
	@szEndMsg	VARCHAR(1024),
	@szValue	VARCHAR(1024)
AS
	
	DELETE TEVENTCHART WHERE dwIndex = @dwIndex
	IF( @bType  > 0)	
	BEGIN
		INSERT INTO TEVENTCHART(
				dwIndex,
				bID,
				szTitle,
				bGroupID,
				bSvrType,
				bSvrID,
				dStartDate,
				dEndDate,
				wValue,
				wMapID,
				dwStartAlarm,
				dwEndAlarm,
				szStartMsg,
				szMidMsg,
				szEndMsg,
				szValue,
				bPartTime)  VALUES (
				@dwIndex,
				@bID,
				@szTitle,
				@bGroupID,
				@bSvrType,
				@bSvrID,
				@dStartDate,
				@dEndDate,
				@wValue,
				@wMapID,
				@dwStartAlarm,
				@dwEndAlarm,
				@szStartMsg,
				@szMidMsg,
				@szEndMsg,
				@szValue,
				@bPartTime ) 		
	END

	RETURN 0


