#include "stdafx.h"
#include "TClientGame.h"
#include "CompStyleDlg.h"
#include "TCompanionInner.h"

#define GAUGE_MOVING_IN_SECS	(3 * 1000)

CCompStyleDlg::CCompStyleDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	m_pFinishTxt = FindKid(26953);
	m_pFaceIcon = (TImageList*) FindKid(11106);
	m_pTargetIcon = (TImageList*) FindKid(28171);

	m_pStart = (TButton*) FindKid(21);
	m_pGauge = (TGauge*) FindKid(26925);
	m_pGauge->SetStyle(TGS_GROW_RIGHT);

	RemoveKid(FindKid(26962));
	AddKid(FindKid(26962));

	RemoveKid(m_pFaceIcon);
	AddKid(m_pFaceIcon);

	RemoveKid(m_pFinishTxt);
	AddKid(m_pFinishTxt);

	m_pFaceIcon->MoveComponentBy(0, -3);

	FindKid(19)->m_strText = "Transfer Companion";
	m_pStart->m_strText = "Transfer";
	m_pFinishTxt->m_strText = "The companion was successfully transfered.";
	m_pFinishTxt->ShowComponent(FALSE);

	m_bProgress = FALSE;
	m_bFinished = FALSE;

	m_bInvenID = INVEN_DEFAULT;
	m_bItemID = 0;
}

CCompStyleDlg::~CCompStyleDlg()
{

}

BOOL CCompStyleDlg::CanWithItemUI()
{
	return TRUE;
}

BYTE CCompStyleDlg::InitData(BYTE bSlot, WORD wToMonID, BYTE bInvenID, BYTE bItemID)
{
	m_bFromSlot = bSlot;
	m_wToMonID = wToMonID;
	m_bInvenID = bInvenID;
	m_bItemID = bItemID;
	m_bFinished = FALSE;

	CTCompanionDlg* pDlg = static_cast<CTCompanionDlg*> (CTClientGame::GetInstance()->GetFrame(TFRAME_COMPANION));
	CTClientCompanion* pCompanion = pDlg->GetSelectedCompanion(bSlot);
	if (!pCompanion)
		return FALSE;

	LPTMONTEMP pFromMonster = CTChart::FindTMONTEMP(pCompanion->GetMonID());
	if (!pFromMonster)
		return FALSE;

	m_pFaceIcon->SetCurImage(pFromMonster->m_wFaceIcon);

	LPTMONTEMP pToMonster = CTChart::FindTMONTEMP(wToMonID);
	if (!pToMonster)
		return FALSE;

	m_pTargetIcon->SetCurImage(pToMonster->m_wFaceIcon);
	ShowComponent(TRUE);

	CPoint pt;
	pDlg->GetComponentPos(&pt);
	pt.x += 400;
	if (pt.x + 100 < CTClientGame::GetInstance()->GetScreenX())
		MoveComponent(pt);

	return TRUE;
}

HRESULT CCompStyleDlg::Render(DWORD dwTickCount)
{
	static BOOL bProgress = m_bProgress;
	static DWORD dwTick = 0;

	if (m_bFinished)
	{
		m_pFinishTxt->m_strText = "The companion was transfered.";
		m_pStart->m_strText = "Close";

		return CTClientUIBase::Render(dwTickCount);
	}
	else 
	{
		m_pStart->m_strText = "Transfer";
		m_pFinishTxt->m_strText = "Click Transfer to start.";
	}

	if (bProgress != m_bProgress)
	{
		dwTick = 0;
		bProgress = m_bProgress;
	}

	if (bProgress)
	{
		dwTick += dwTickCount;
		m_pGauge->SetGauge(dwTick, GAUGE_MOVING_IN_SECS);
		m_pFinishTxt->m_strText.Empty();

		if (dwTick >= GAUGE_MOVING_IN_SECS)
		{
			m_bProgress = FALSE;
			dwTick = 0;

			m_bFinished = TRUE;
			m_pFaceIcon->SetCurImage(m_pTargetIcon->GetCurImage());
			
			CTClientSession* pSession = CTClientGame::GetInstance()->GetSession();
			if (pSession)
				pSession->SendCS_FINISHCOMPANIONTRANSFER_ACK(m_bFromSlot, m_bInvenID, m_bItemID);
		}
	}
	else
		m_pGauge->SetGauge(0, GAUGE_MOVING_IN_SECS);

	return CTClientUIBase::Render(dwTickCount);
}

void CCompStyleDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	if (m_pStart->HitTest(pt) && IsVisible()) {
		if (m_pStart->m_strText != "Close")
			m_bProgress = TRUE;
		else
			ShowComponent(FALSE);
	}

	return CTClientUIBase::OnLButtonUp(nFlags, pt);
}

void CCompStyleDlg::ShowComponent(BOOL bVisible)
{
	if (m_bProgress)
		m_bProgress = FALSE;
	
	return CTClientUIBase::ShowComponent(bVisible);
}