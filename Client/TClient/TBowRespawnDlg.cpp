#include "stdafx.h"
#include "TBowRespawnDlg.h"
#include "TClientGame.h"
#include "TClientWnd.h"

CTBowRespawnDlg::CTBowRespawnDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	m_pTime = (TButton*) FindKid(28155);
	m_pPrice = FindKid(27451);
	static_cast<TButton*>(FindKid(ID_CTRLINST_OK))->m_menu[TNM_LCLICK] = GM_BOW_CASH_RESPAWN;

	SetTime(0);
}

CTBowRespawnDlg::~CTBowRespawnDlg()
{

}

BOOL CTBowRespawnDlg::CanWithItemUI()
{
	return TRUE;
}

void CTBowRespawnDlg::SetTime(DWORD dwTick)
{
	m_pTime->m_strText.Format("%d", dwTick / 1000);
}

void CTBowRespawnDlg::ShowComponent(BOOL bVisible)
{
	CTClientGame* pGAME = CTClientGame::GetInstance();
	CTClientChar* pCHAR = pGAME->GetMainChar();

	if (pCHAR)
	{
		if (CTClientGame::IsInBRMap())
			m_pPrice->m_strText.Format("%d", BOW_RESPAWN_MEDAL_BASE + BOW_RESPAWN_MEDAL_ADDER);
		else
			m_pPrice->m_strText.Format("%d", BOW_RESPAWN_MEDAL_BASE + (pCHAR->m_bDeaths * BOW_RESPAWN_MEDAL_ADDER));
	}

	CTClientUIBase::ShowComponent(bVisible);
}
