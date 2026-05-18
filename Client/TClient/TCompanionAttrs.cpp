#include "StdAfx.h"
#include "TCompanionAttrs.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TCompanionInner.h"

#define ID_PET_STRVAL (0x00006685)
#define ID_PET_INTVAL (0x00006686)
#define ID_PET_AGIVAL (0x00006687)
#define ID_PET_WISVAL (0x00006688)
#define ID_PET_ENDVAL (0x00006813)
#define ID_PET_WILLVAL (0x00006814)

#define ID_PET_STRTEXT (0x00006815)
#define ID_PET_INTTEXT (0x00006816) 
#define ID_PET_AGITEXT (0x00006817)
#define ID_PET_WISTEXT (0x00006818) 
#define ID_PET_ENDTEXT (0x00006819)
#define ID_PET_WILLTEXT (0x0000681A) 

#define ID_PET_STRBUT (0x0000667F)
#define ID_PET_INTBUT (0x00006680)
#define ID_PET_AGIBUT (0x00006681)
#define ID_PET_WISBUT (0x00006682)
#define ID_PET_ENDBUT (0x0000681B)
#define ID_PET_WILLBUT (0x0000681C)

#define ID_PET_STATUI (0x000064AF)
#define ID_PET_STATTEXT (0x0000667E)
#define ID_PET_STATVALUE (0x0000668B)
#define ID_PET_BONUS1T (0x00000FBB)
#define ID_PET_BONUS2T (0x00000FDF)
#define ID_PET_BONUS3T (0x00000FE0)
#define ID_PET_BONUS4T (0x00000FE1)
#define ID_PET_BONUS5T (0x00000FE2)
/**/
#define ID_PET_BONUS1V (0x00000FF3)
#define ID_PET_BONUS2V (0x00000FF2)
#define ID_PET_BONUS3V (0x00000FEF)
#define ID_PET_BONUS4V (0x00000FD3)
#define ID_PET_BONUS5V (0x00000FD4)

CTCompanionAttrs::CTCompanionAttrs( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
	: ITInnerFrame(pParent, pDesc, TCOMPANION_STATS)
{
	static DWORD dwStatT[6] =
	{
		ID_PET_STRTEXT,	//TCHARSTAT_STR
		ID_PET_AGITEXT,	//TCHARSTAT_DEX
		ID_PET_ENDTEXT,	//TCHARSTAT_CON
		ID_PET_INTTEXT,	//TCHARSTAT_INT
		ID_PET_WISTEXT,	//TCHARSTAT_WIS
		ID_PET_WILLTEXT	//TCHARSTAT_MEN
	};

	static DWORD dwStatB[6] =
	{
		26239,
		26241,
		26645,
		26240,
		26242,
		26646
	};

    static DWORD dwStatV[6] =
	{
		26245,	//TCHARSTAT_STR
		26247,	//TCHARSTAT_DEX
		26637,	//TCHARSTAT_CON
		26246,	//TCHARSTAT_INT
		26248,	//TCHARSTAT_WIS
		26638	//TCHARSTAT_MEN
	};

	m_pStatPText = static_cast<TComponent*>(FindKid(ID_PET_STATTEXT));
	m_pStatPValue = static_cast<TComponent*>(FindKid(26251));
	for( BYTE i=0; i < TCHARSTAT_COUNT; ++i)
		m_pStatT[i] = static_cast<TComponent*>(FindKid( dwStatT[i] ));

	for( BYTE i=0; i < TCHARSTAT_COUNT; ++i)
	{
		m_pStatV[i] = static_cast<TComponent*>(FindKid( dwStatV[i] ));
		m_pStatV[i]->m_strText = "0 + (0)";
	}

	for( BYTE i=0; i < TCHARSTAT_COUNT; ++i)
		m_pStatB[i] =  static_cast<TButton*>(FindKid( dwStatB[i] ));

	m_pStatPValue->m_strText = "0";
}

CTCompanionAttrs::~CTCompanionAttrs()
{
}

void CTCompanionAttrs::OnLButtonUp(UINT nFlags, CPoint pt)
{
	CTClientGame* pGame = CTClientGame::GetInstance();
	CTCompanionDlg* pStatic = static_cast<CTCompanionDlg*>(pGame->GetFrame(TFRAME_COMPANION));
	CTClientCompanion* m_pSelCompanion = pStatic->GetSelectedCompanion(pStatic->m_bCurSelSlot);
		if( !m_pSelCompanion || pStatic->IsCompanionEmpty() )
		return;

	for( BYTE i = 0; i < 6; ++i )
	{
		if(m_pStatB[ i ]->HitTest( pt ) )
		{
			pGame->GetSession()->SendCS_COMPANIONUPGRADE_REQ( i, pStatic->m_bCurSelSlot );
			break;
		}
	}

	CTClientUIBase::OnLButtonUp(nFlags,pt);
}
