
CREATE PROCEDURE [dbo].[TFourYearEventPost]
@dwCharID INT
AS

DECLARE @szName VARCHAR(50)

SELECT @szName = szName  FROM TCHARTABLE WHERE dwCharID = @dwCharID
IF(@@ROWCOUNT = 0 OR @szName IS NULL)
	RETURN

IF NOT EXISTS(SELECT dwSeq FROM TRESERVEDPOST WHERE dwRecverID=@dwCharID AND wItemID = 9400)
BEGIN
	EXEC TEventItemGive 
		@szName,
		9400,
		1,
		'The 4th Anniversary of 4Story has come!',
'We will be celebrating 4th anniversary of 4Story as an event. Tablets with letters <4>, <S>, <TO>, and <RY> will be dropped from every monsters of Iveria. Collect a set (4Story) of tablets and bring them to [Event Manager]"Master Shin" in Eden Square of Thebekut. Master Shin will exchange the tablets to <Interesting Gift>.
<Interesting Gift> can be unsealed through "Elf Sorcerer" in town. You can obtain various items when you unseal <Interesting Gift>, and can exchange tablets as many as you can to get <Interesting Gift>s.
Enjoy the event and have a great time with 4Story Online! Thank you.'
END


