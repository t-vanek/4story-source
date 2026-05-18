CREATE PROCEDURE [dbo].[RandomColor]


@dwCharID  INT


AS
BEGIN

DECLARE @Random INT
DECLARE @Upper INT
DECLARE @Lower INT
DECLARE @bClass TINYINT
DECLARE @bLevel	INT
DECLARE @bGem TINYINT

SET @bGem = 5
SET @bLevel = 24 /* upgrade +18 */
SET @Random = 33 /* Effect */


SELECT  @bClass = bClass  FROM TCHARTABLE WHERE dwCharID = @dwCharID


IF(@bClass <= 2)/* Warrior,Archer,Assassin*/
BEGIN
        
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 0 AND dwStorageID = 254
		
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 1 AND dwStorageID = 254
		
		
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 2 AND dwStorageID = 254

		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 3 AND dwStorageID = 254
		
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 5 AND dwStorageID = 254
	
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 6 AND dwStorageID = 254
		
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 7 AND dwStorageID = 254
		
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 8 AND dwStorageID = 254
		

END	

IF(@bClass >= 3)/* Priest,Mage,Summoner*/
BEGIN




		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 0 AND dwStorageID = 254

		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 3 AND dwStorageID = 254
	
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 5 AND dwStorageID = 254
		
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 6 AND dwStorageID = 254
		
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 7 AND dwStorageID = 254
	
		UPDATE TITEMTABLE SET bGem = @bGem, bLevel = @bLevel, bGradeEffect = @Random WHERE dwOwnerID = @dwCharID AND bItemID = 8 AND dwStorageID = 254

END	

     END


