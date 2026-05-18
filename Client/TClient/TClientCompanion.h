#pragma once


class CTClientCompanion
{
protected:
	WORD		m_wMonID;

	BYTE		m_bSlot;
	CString		m_strCompanionName;

	DWORD	m_dwExp;
	DWORD	m_dwNextExp;
	DWORD	m_dwLife;
	BYTE	m_bLevel;
	WORD	m_wAttrPoint;

	WORD	m_wBonusID;
	FLOAT	m_fBonusValue;
	CString	m_strBonusName;
	WORD    m_wItemID[2];
	BYTE	m_wCharAttr[ TCHARSTAT_COUNT ];

public:

	void SetCompanionInfo( WORD wMonID, CString strName, DWORD dwExp, DWORD dwNextExp, DWORD dwLife, BYTE bLevel, WORD wAttrPoint );

	void SetBonusInfo( WORD wBonusID, FLOAT fBonusValue );

	void SetCharATTR( BYTE wCharAttr[] );

	void SetSlot( BYTE bSlot ) { m_bSlot = bSlot; }

	void UpdateLife( DWORD dwLife ) { m_dwLife = dwLife; }
	void UpdateExp( DWORD dwExp ) { m_dwExp = dwExp; }
	void UpdateNextExp( DWORD dwNextExp ) { m_dwNextExp = dwNextExp; }
	void UpdateLevel( BYTE bLevel ) { m_bLevel = bLevel; }
	void UpdateCharATTR( BYTE bType, BYTE wValue ) { m_wCharAttr[bType] = wValue; }
	void UpdateAttrPoint( WORD wAttrPoint ) { m_wAttrPoint = wAttrPoint; }
	void UpdateBonus( FLOAT fBonusValue ) { m_fBonusValue = fBonusValue; }
	void UpdateItems( BYTE bSlot, CTClientItem* pItem ) { m_pItem[ bSlot ] = pItem; m_wItemID[ bSlot ] = pItem->GetItemID();} 
	void AddCompanionItems( CTClientItem* pItem[ 2 ] );

	BYTE GetSlot()			{ return m_bSlot; }
	DWORD GetExp()			{ return m_dwExp; }
	DWORD GetNextExp()		{ return m_dwNextExp; }
	DWORD GetLife()			{ return m_dwLife; }
	BYTE GetLevel()			{ return m_bLevel; }
	WORD GetAttrPoint()		{ return m_wAttrPoint; }
	WORD GetBonusID()		{ return m_wBonusID; }
	FLOAT GetBonusValue()	{ return m_fBonusValue; }
	BYTE* GetCharATTR()		{ return m_wCharAttr; }

	const CString& GetCompanionName() const	{ return m_strCompanionName; }

	WORD GetMonID()					{ return m_wMonID; }
	DWORD GetItems(BYTE m_bSlot)            { return m_wItemID[m_bSlot]; }

	CTClientItem* m_pItem[2];
	BYTE m_bEffect;
	

public:
	CTClientCompanion();
	virtual ~CTClientCompanion();
};
