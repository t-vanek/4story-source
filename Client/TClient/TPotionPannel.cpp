#include "StdAfx.h"
#include "Resource.h"

#ifdef NEW_IF
#include "TClientGame.h"
#include "TPotionPannel.h"

CTPotionPannel::CTPotionPannel( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
:CTClientUIBase( pParent, pDesc)
{
	static DWORD dwIconID[10] =
	{
		ID_MAINTAIN_1,
			ID_MAINTAIN_2,
			ID_MAINTAIN_3,
			ID_MAINTAIN_4,
			ID_MAINTAIN_5,
			ID_MAINTAIN_6,
			ID_MAINTAIN_7,
			ID_MAINTAIN_8,
			ID_MAINTAIN_9,
			ID_MAINTAIN_10
	};

	static DWORD dwTextID[10] =
	{
		ID_MAINTAIN_TIME_1,
			ID_MAINTAIN_TIME_2,
			ID_MAINTAIN_TIME_3,
			ID_MAINTAIN_TIME_4,
			ID_MAINTAIN_TIME_5,
			ID_MAINTAIN_TIME_6,
			ID_MAINTAIN_TIME_7,
			ID_MAINTAIN_TIME_8,
			ID_MAINTAIN_TIME_9,
			ID_MAINTAIN_TIME_10
	};

	for( BYTE j=0; j<10; j++)
	{
		m_pMAINTAIN_ICON[j] = (TImageList *) FindKid(dwIconID[j]);
		m_pMAINTAIN_TIME[j] = (TComponent*) FindKid(dwTextID[j]);

		if(m_pMAINTAIN_ICON[j])
		{
			m_pMAINTAIN_ICON[j]->EnableComponent(TRUE);
			m_pMAINTAIN_ICON[j]->ShowComponent(TRUE);
			m_pMAINTAIN_ICON[j]->SetCurImage(0);
		}

		m_vMAINTAININFO_TSKILL[j] = NULL;
		m_vMAINTAININFO_LEVEL[j] = 0;
	}

	m_bSND = FALSE;
	m_bCount = 0;
	m_pOBJ = NULL;
}

CTPotionPannel::~CTPotionPannel()
{
}

bool SortCancel( CTClientMaintain* p1, CTClientMaintain* p2 )
{
	if( p1->m_pTSKILL && p2->m_pTSKILL )
	{
		BOOL bPremium1 = IsPREMIUMMaintain(p1);
		BOOL bPremium2 = IsPREMIUMMaintain(p2);

		if( bPremium1 && bPremium2 )
			return true;

		else if( bPremium1 || bPremium2 )
		{
			if( bPremium1 )
				return true;
			else
				return false;
		}
		else
		{
			if( p2->m_pTSKILL->m_bCanCancel && !p1->m_pTSKILL->m_bCanCancel)
				return false;
		}
	}

	return true;
}

void CTPotionPannel::ResetPOTIONS( CTClientObjBase* pOBJ, DWORD dwTick)
{
/*	
	HideAll();

	m_pOBJ = pOBJ;

	LPMAPTMAINTAIN pTMAINTAIN = pOBJ->GetTMAINTAIN();
	VTMAINTAIN vTMAINTAIN;
	VTMAINTAIN vTPRotation;
	VTMAINTAIN vTNRotation;

	{
		MAPTMAINTAIN::iterator itMAINTAIN;
		for( itMAINTAIN = pTMAINTAIN->begin(); itMAINTAIN != pTMAINTAIN->end(); itMAINTAIN++)
			vTMAINTAIN.push_back( itMAINTAIN->second );

		std::sort( vTMAINTAIN.begin(), vTMAINTAIN.end(), SortCancel);
	}

	VTMAINTAIN::iterator itMAINTAIN;

	for( int i=0; i<vTMAINTAIN.size(); ++ i )
	{
		CTClientMaintain *pTSKILL = vTMAINTAIN[i];

		if( pTSKILL && pTSKILL->m_pTSKILL && pTSKILL->m_pTSKILL->m_bShowIcon )
		{
			BYTE bTMCNT = m_bCount;
			TImageList* pTIMGLIST = m_pMAINTAIN_ICON[bTMCNT];
			TComponent* pTTIME = m_pMAINTAIN_TIME[bTMCNT];

			if( bTMCNT < 10 && pTIMGLIST )
			{
				if(pTSKILL->GetLeftTick(dwTick) > 10)
				{
					pTTIME->m_strText = CTClientGame::ToTimeOneString(pTSKILL->GetLeftTick(dwTick));
					pTTIME->ShowComponent(TRUE);
				}
				else
				{
					pTTIME->m_strText.Empty();
					pTTIME->ShowComponent(FALSE);
				}

				if( pTSKILL->m_pTSKILL->m_wIconID != 0 )
				{
					pTIMGLIST->SetCurImage(pTSKILL->m_pTSKILL->m_wIconID);
					pTIMGLIST->ShowComponent(TRUE);
				}
				else
					pTIMGLIST->ShowComponent(FALSE);

				m_vMAINTAININFO_TSKILL[ bTMCNT ] = pTSKILL->m_pTSKILL;
				m_vMAINTAININFO_LEVEL[ bTMCNT ] = pTSKILL->m_bLevel;
			}
			++m_bCount;
		}
	}

	for( BYTE j=m_bCount ; j<10; ++j)
	{
		if( m_pMAINTAIN_ICON[j] )
			m_pMAINTAIN_ICON[j]->ShowComponent(FALSE);
		if( m_pMAINTAIN_TIME[j] )
			m_pMAINTAIN_TIME[j]->ShowComponent(FALSE);
	}
	*/
}

void CTPotionPannel::HideAll()
{
	
	for( BYTE j=0; j<10; ++j)
	{
		if(m_pMAINTAIN_ICON[j])
		{
			m_pMAINTAIN_TIME[j]->m_strText.Empty();
			m_pMAINTAIN_TIME[j]->ShowComponent(FALSE);

			m_pMAINTAIN_ICON[j]->SetCurImage(0);
			m_pMAINTAIN_ICON[j]->ShowComponent(FALSE);

			m_vMAINTAININFO_TSKILL[j] = NULL;
			m_vMAINTAININFO_LEVEL[j] = 0;
		}
	}
	
}

HRESULT CTPotionPannel::Render(DWORD dwTickCount)
{
	CPoint pt;
	CTClientGame::GetInstance()->m_vTGAUGEFRAME[TGAUGE_FRAME_PLAYER]->GetComponentPos(&pt);
	MoveComponent(CPoint(pt.x + 5, pt.y + 65));

	if(m_bIsHidden || CTClientGame::IsInBOWMap() || CTClientGame::IsInBRMap())
		ShowComponent(FALSE);

	return CTClientUIBase::Render(dwTickCount);
}

ITDetailInfoPtr CTPotionPannel::GetTInfoKey( const CPoint& point )
{

	ITDetailInfoPtr pInfo(NULL);

	for( BYTE j=0; j<10; ++j)
	{
		if(m_pMAINTAIN_ICON[j] &&
			m_pMAINTAIN_ICON[j]->HitTest(point) )
		{
			CRect rc;
			m_pMAINTAIN_ICON[j]->GetComponentRect(&rc);
			m_pMAINTAIN_ICON[j]->ComponentToScreen(&rc);

			pInfo = CTDetailInfoManager::NewSkillInst( 
				m_vMAINTAININFO_TSKILL[j],
				m_vMAINTAININFO_LEVEL[j],
				FALSE,
				rc);

			pInfo->SetDir(TRUE, FALSE, TRUE);
			return pInfo;
		}
	}

	return pInfo;
}

BOOL CTPotionPannel::HitTest( CPoint pt)
{
	
	for( BYTE j=0; j<10; j++)
		if( m_pMAINTAIN_ICON[j] &&
			m_pMAINTAIN_ICON[j]->HitTest(pt) )
			return TRUE;
			
	return FALSE;
}

void CTPotionPannel::AddMaintain(BYTE bIndex, BYTE bIcon, DWORD dwTick, CTClientMaintain* pSkill, CTClientObjBase* pChar)
{
	bIndex-=1;

	if(!m_bIsHidden && !CTClientGame::IsInBOWMap() && !CTClientGame::IsInBRMap())
		ShowComponent(TRUE);

	m_vMAINTAININFO_TSKILL[ bIndex ] = pSkill->m_pTSKILL;
	m_vMAINTAININFO_LEVEL[ bIndex ] = pSkill->m_bLevel;

	for(BYTE i=bIndex;i<10; i++)
	{
		m_pMAINTAIN_ICON[i]->ShowComponent(FALSE);
		m_pMAINTAIN_TIME[i]->ShowComponent(FALSE);
		m_pMAINTAIN_TIME[i]->m_strText.Empty();
		m_pMAINTAIN_ICON[i]->SetCurImage(-1);
	}

	if(pSkill && pSkill->m_pTSKILL->m_bPremIconID != 0 && m_vMAINTAININFO_TSKILL[bIndex]->m_bPremIconID != 0 )
	{
		if(pSkill->GetLeftTick(dwTick) != 0 && pSkill->m_pTSKILL->m_bShowTime)
		{
			m_pMAINTAIN_TIME[bIndex]->m_strText = CTClientGame::ToTimeOneString(pSkill->GetLeftTick(dwTick));
			m_pMAINTAIN_TIME[bIndex]->ShowComponent(TRUE);
		}
		else
		{
			m_pMAINTAIN_TIME[bIndex]->m_strText.Empty();
			m_pMAINTAIN_TIME[bIndex]->ShowComponent(FALSE);
		}
	}
	else
	{
		m_pMAINTAIN_TIME[bIndex]->m_strText.Empty();
		m_pMAINTAIN_TIME[bIndex]->ShowComponent(FALSE);
	}

	m_pMAINTAIN_ICON[bIndex]->SetCurImage(bIcon-1);
	m_pMAINTAIN_ICON[bIndex]->ShowComponent(TRUE);
	
}


void CTPotionPannel::SetNewChar(CTClientObjBase* pMainChar)
{
	m_pMainChar = pMainChar;
}

#endif