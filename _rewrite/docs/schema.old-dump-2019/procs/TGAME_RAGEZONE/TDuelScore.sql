

/* 
========================================================
RETURN VALUE
========================================================
0	: SUCCESS
1	: INVALID CHARACTER

*/


CREATE PROCEDURE [dbo].[TDuelScore]	
	@dwCharID		INT,	
	@dwWarriorLose	INT,
	@dwWarriorWin		INT,
	@dwRangerLose	INT,	
	@dwRangerWin		INT,
	@dwArcherLose		INT,	
	@dwArcherWin		INT,
	@dwWizardLose	INT,
	@dwWizardWin		INT,
	@dwPriestLose		INT,
	@dwPriestWin		INT,
	@dwSorcererLose	INT,
	@dwSorcererWin	INT	
AS
	SELECT dwCharID FROM TCHARTABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT = 0 )
		RETURN 1

	SELECT dwCharID FROM TDUELSCORETABLE WHERE dwCharID = @dwCharID
	IF(@@ROWCOUNT > 0 )
		DELETE FROM TDUELSCORETABLE WHERE dwCharID = @dwCharID
	
	INSERT INTO TDUELSCORETABLE(
		dwCharID,	
		dwWarriorWin,
		dwWarriorLose,
		dwRangerWin,
		dwRangerLose,
		dwArcherWin,
		dwArcherLose,
		dwWizardWin,
		dwWizardLose,
		dwPriestWin,
		dwPriestLose,
		dwSorcererWin	,
		dwSorcererLose	 )
	VALUES (
		@dwCharID,	
		@dwWarriorWin	,
		@dwWarriorLose,
		@dwRangerWin	,
		@dwRangerLose,
		@dwArcherWin	,
		@dwArcherLose	,
		@dwWizardWin	,
		@dwWizardLose,
		@dwPriestWin,
		@dwPriestLose,
		@dwSorcererWin,
		@dwSorcererLose )

	RETURN 0



