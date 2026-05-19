/*
	OP Tool Manager Log

*/
CREATE PROCEDURE [dbo].[OPTool_ManagerLog]
	@pIP		VARCHAR(15),
	@pGMID	VARCHAR(15),
	@pCommand	VARCHAR(20),
	@pLog		VARCHAR(1024)
AS

	INSERT INTO	TMANAGERLOG
	(
		szIP,
		szGMID,
		szCommand,
		szLog
	)
	VALUES
	(
		@pIP,
		@pGMID,
		@pCommand,
		@pLog
	)

