
CREATE PROCEDURE [dbo].[TScammingPost]
@dwCharID INT
AS

DECLARE @szName VARCHAR(50)
DECLARE @bCountry TINYINT

SELECT @szName = szName, @bCountry=bCountry FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0 OR @szName IS NULL)
	RETURN

IF NOT EXISTS(SELECT dwSeq FROM TRESERVEDPOST WHERE dwRecverID=@dwCharID AND wItemID = 9400)
BEGIN

	EXEC TEventItemGive 
		@szName,
		9400,
		1,
		'How to avoid scamming',
		'Recently, there are lots of scamming inquiries to 4Story GM Team.
Thus, we would like to clearly introduce “How to avoid scammings”

1. Please do not ask upgrading, enchants, effect changing and etc to other players.
2. Do not trust a person who approaches to you by requesting your items and money for upgrading, enchanting and etc.
3. Do not trade Premium Shop items with game money or items.
4. Please be extra more careful when someone sent you a bill. You must make sure the item is attached first before you accept it. Also check if the item is correct. Item name on billing titles and attachment can be different.
5. Please be extra more careful to trade, when the opponent cancels the trade and request trade to you. You must confirm the items or money are unchanged, before you trade.
6. Please make sure to double-check spelling on confusing character names.
7. Do not rent items to a person who you cannot fully trust.
8. Do not share your account access information (game account, e-mail account, and password) with anyone else.

Please remind all the subjects above to avoid all kinds of disadvantages as a result of scamming. 4Story GM team will not take any responsibilities and actions to recover scammed items or game money when you did not pay enough attention with the subjects that are written here.
 
Thank you for playing 4Story
 
4Story GM Team
'

END


