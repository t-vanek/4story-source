#include "StdAfx.h"
#include "DailyRewardPannel.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "TCompanionMiniUI.h"

CDailyRewardPannel::CDailyRewardPannel(TComponent *pParent, LP_FRAMEDESC pDesc)
: CTClientUIBase(pParent, pDesc)
{
	CTClientGame* pGAME = CTClientGame::GetInstance();
	CTCompanionMiniUI* pDlg = static_cast<CTCompanionMiniUI*>(pGAME->GetFrame(TFRAME_COMPANION));
	m_pItem[0] = new TImageList(this, pDlg->m_pItems[0]->m_pDESC);
	m_pGauge[0] = new TGauge(this, pDlg->m_pCooldowns[0]->m_pDESC);
	m_pChoosen->UseOwnImages(25010);

	CPoint ptFirst;
	m_pItem[0]->GetComponentPos(&ptFirst);
	m_pChoosen->MoveComponent(ptFirst);

	for (BYTE i = 1; i < MAX_DAILY_REWARD; ++i)
	{
		m_pItem[i] = new TImageList(this, m_pItem[0]->m_pDESC);
		m_pGauge[i] = new TGauge(this, m_pGauge[0]->m_pDESC);

		m_pItem[i]->MoveComponentBy(7 * i, 0);
		m_pGauge[i]->MoveComponentBy(7 * i, 0);

		m_pItem[i]->m_id = GetUniqueID();
		m_pGauge[i]->m_id = GetUniqueID();

		AddKid(m_pItem[i]);
		AddKid(m_pGauge[i]);
	}

}

CDailyRewardPannel::~CDailyRewardPannel()
{

}
