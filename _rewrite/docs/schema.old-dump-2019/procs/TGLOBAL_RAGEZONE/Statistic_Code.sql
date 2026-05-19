create  PROCEDURE [dbo].[Statistic_Code]
	@pCodeLarge	VARCHAR(20)
 AS

	SELECT 	CODE_LABEL,
			CODE_VALUE
	FROM	SCODETL
	WHERE CODE_LARGE = @pCodeLarge





