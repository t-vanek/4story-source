#include "stdafx.h"
#include "TBowBPTradeDlg.h"
#include "TClientGame.h"
#include "TClientWnd.h"

DWORD CTBowBPTradeDlg::m_dwMedals = 0;
DWORD CTBowBPTradeDlg::m_dwBP = 0;

CTBowBPTradeDlg::CTBowBPTradeDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	m_pTradeDesc = FindKid(16450);
	m_pMedalCost = FindKid(28162);

	m_pTotalMedals = FindKid(28161);

	m_pMin = FindKid(28158);
	m_pMax = FindKid(28157);

	m_pMin->m_strText = "0";

	m_pBuy = static_cast<TButton*>(FindKid(ID_CTRLINST_OK));
	m_pRange = static_cast<TScroll*>(FindKid(ID_CTRLINST_BAR));

	m_pBuy->m_menu[TNM_LCLICK] = GM_BP_EXCHANGE;
	m_pRange->SetScrollType(FALSE);
}

CTBowBPTradeDlg::~CTBowBPTradeDlg()
{

}

void CTBowBPTradeDlg::UpdateScroll() 
{
	CTGaugePannel* pGaugePannel = static_cast<CTGaugePannel*>(CTClientGame::GetInstance()->GetFrame(TFRAME_GAUGE));
	int nRng, nPos;

	m_pRange->GetScrollPos(nRng, nPos);

	DWORD Medals = atoi(pGaugePannel->m_pMedals->m_strText);

	m_dwMedals = (DWORD) FLOAT(((FLOAT)nPos / (FLOAT)nRng) * min(100, Medals));
	m_dwBP = m_dwMedals * MEDAL_BP_RATE;

	m_pTradeDesc->m_strText.Format("You are going to buy %d BP for %d Medal(s).", m_dwBP, m_dwMedals);
	m_pTotalMedals->m_strText.Format("%d", min(100, Medals));
	m_pMedalCost->m_strText.Format("%d", m_dwMedals);
	m_pMax->m_strText.Format("%d", Medals * MEDAL_BP_RATE);
}

HRESULT CTBowBPTradeDlg::Render(DWORD dwTickCount)
{
	if (IsVisible())
		UpdateScroll();

	return CTClientUIBase::Render(dwTickCount);
}