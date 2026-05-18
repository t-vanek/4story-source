#include "Stdafx.h"
#include "TClientCompanion.h"
#include "TClientGame.h"
#include "TPetManageDlg.h"

// =======================================================
CTClientCompanion::CTClientCompanion()
{
	for( BYTE i = 0; i < TCHARSTAT_COUNT; ++i )
		m_wCharAttr[ i ] = 0;

	for(auto i=0;i<2;++i)
	{
		m_pItem[ i ] = NULL;
		m_wItemID[ i ] = 0;
	}
}
// -------------------------------------------------------
CTClientCompanion::~CTClientCompanion()
{
	
}

// =======================================================
void CTClientCompanion::SetCompanionInfo(WORD wMonID, CString strName, DWORD dwExp, DWORD dwNextExp, DWORD dwLife, BYTE bLevel, WORD wAttrPoint)
{
	m_wMonID = wMonID;

	m_strCompanionName = strName;
	m_dwExp = dwExp;
	m_dwNextExp = dwNextExp;
	m_dwLife = dwLife;
	m_bLevel = bLevel;
	m_wAttrPoint = wAttrPoint;
}

void CTClientCompanion::AddCompanionItems( CTClientItem* pItem[ 2 ] )
{
	for( BYTE i = 0; i < 2; ++i )
	{
		m_pItem[ i ] = pItem[ i ];
		m_wItemID[ i ] = pItem[ i ]->GetItemID();
	}
}


void CTClientCompanion::SetBonusInfo(WORD wBonusID, FLOAT fBonusValue)
{
	m_wBonusID = wBonusID;
	m_fBonusValue = fBonusValue;

}

void CTClientCompanion::SetCharATTR( BYTE wCharAttr[] )
{
	for( BYTE i = 0; i < TCHARSTAT_COUNT; ++i )
		m_wCharAttr[ i ] = wCharAttr[ i ];
}