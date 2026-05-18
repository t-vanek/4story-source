#include "stdafx.h"
#include "TSoulLotteryWinDlg.h"
#include "TClientGame.h"

#define ID_GOLDY (0x00006293)
#define ID_STRIBRA (0x00006294)
#define ID_BRONZY (0x00006295)
#define ID_SKILLTEXT (0x00006825)
#define ID_OK_BUT (0x00006828)

CTLotWinDlg::CTLotWinDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	m_pGold = FindKid(ID_GOLDY);
	m_pSilver = FindKid(ID_STRIBRA);
	m_pCron = FindKid(ID_BRONZY);
	m_pSText = FindKid(ID_SKILLTEXT);
	m_pOK = static_cast<TButton*>(FindKid(ID_OK_BUT));

	m_pSText->m_strText = "Hunting EP: +150% (15 min.)";

}

BOOL CTLotWinDlg::CanWithItemUI()
{
	return TRUE;
}

void CTLotWinDlg::SetReward(DWORD dwMoney)
{
	m_pGold->m_strText = CTChart::Format( TSTR_FMT_NUMBER, dwMoney / TRUNE_DIVIDER);
	m_pSilver->m_strText = CTChart::Format( TSTR_FMT_NUMBER, (dwMoney % TRUNE_DIVIDER) / TLUNA_DIVIDER);
	m_pCron->m_strText = CTChart::Format( TSTR_FMT_NUMBER, dwMoney % TLUNA_DIVIDER);
}

void CTLotWinDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	if (m_pOK->HitTest(pt))
	{
		CTClientGame::GetInstance()->GetFrame(TFRAME_SOULLOTTERY)->ShowComponent(FALSE);
		ShowComponent(FALSE);
	}

	CTClientUIBase::OnLButtonUp(nFlags, pt);
}

CTLotWinDlg::~CTLotWinDlg()
{

}
