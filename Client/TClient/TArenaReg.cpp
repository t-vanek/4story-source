#include "stdafx.h"
#include "Resource.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "TArenaReg.h"

static const BYTE BSize = 27 + 3; 

CTArenaRegDlg::CTArenaRegDlg( TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc)
: CTClientUIBase( pParent, pDesc)
{
	m_pTSCROLL = static_cast<TScroll*>(static_cast<TList*>(FindKid(875))->FindKid(47050));
	m_pUComp = (TButton*)FindKid(26183);
	m_pDComp = (TButton*)FindKid(26184);

	m_pUWinCount = FindKid(26185);
	m_pULevel = FindKid(18645);
	m_pGroupUName = FindKid(863);

	m_pGroupUC = (TImageList*) FindKid(25036);
	/* ======================= LOADING OF SECONDS COMPONENTS ======================= */
	{
		CPoint ptWin, ptLevel, ptName, ptCountry;

		m_pDWinCount = new TComponent(this, m_pUWinCount->m_pDESC);
		m_pDWinCount->m_id = GetUniqueID();
		m_pUWinCount->GetComponentPos(&ptWin);
		ptWin.y += BSize;

		m_pDWinCount->MoveComponent(ptWin);
		AddKid(m_pDWinCount);

		m_pDLevel = new TComponent(this, m_pULevel->m_pDESC);
		m_pDLevel->m_id = GetUniqueID();
		m_pULevel->GetComponentPos(&ptLevel);
		ptLevel.y += BSize;

		m_pDLevel->MoveComponent(ptLevel);
		AddKid(m_pDLevel);

		m_pGroupDName = new TComponent(this, m_pGroupUName->m_pDESC);
		m_pGroupDName->m_id = GetUniqueID();
		m_pGroupUName->GetComponentPos(&ptName);

		ptName.y += BSize;
		m_pGroupDName->MoveComponent(ptName);
		AddKid(m_pGroupDName);

		m_pGroupDC = new TImageList(this, m_pGroupUC->m_pDESC);
		m_pGroupDC->m_id = GetUniqueID();
		m_pGroupUC->GetComponentPos(&ptCountry);
		
		ptCountry.y += BSize;
		m_pGroupDC->MoveComponent(ptCountry);
		AddKid(m_pGroupDC);
	}
	/* ======================= LOADING OF QUEUE ======================= */
	{
		CPoint ptWin, ptLevel, ptName, ptCountry;
	
		BYTE Index = 0;
		for (BYTE i = 0; i < 4; ++i)
		{
			Index = i + 1;

			m_pComps[i] = new TButton(this, m_pDComp->m_pDESC);
			m_pComps[i]->m_id = GetUniqueID();

			CPoint ptMove;
			m_pDComp->GetComponentPos(&ptMove);

			ptMove.x = 32;
			ptMove.y = 210 + (Index*BSize);

			m_pComps[i]->MoveComponent(ptMove);
			AddKid(m_pComps[i]);

			m_pQUEUEName[i] = new TComponent(this, m_pGroupDName->m_pDESC);
			m_pQUEUEName[i]->m_id = GetUniqueID();
			
			m_pGroupDName->GetComponentPos(&ptName);
			ptName.x -= 12 + 83;
			ptName.y = 212 + (Index*BSize);

			m_pQUEUEName[i]->MoveComponent(ptName);
			AddKid(m_pQUEUEName[i]);
			m_pQUEUEName[i]->SetTextAlign(ALIGN_CENTER);
		}
	}
	m_nListTop = 0;

	for (BYTE i = 0; i < ARENA_MAX_TEAM; ++i)
		m_ArenaPlayer[i] = nullptr;
	m_Queue.clear();

	m_pTSCROLL->SetScrollType(TRUE);
}

CTArenaRegDlg::~CTArenaRegDlg()
{
	for (BYTE i = 0; i < ARENA_MAX_TEAM; ++i)
		SAFE_DELETE(m_ArenaPlayer[i]);
	m_Queue.clear();
}

void CTArenaRegDlg::AddTeam(CString ChiefName, BYTE Level, BYTE Country, BYTE Wins)
{
	if (m_ArenaPlayer[0] && m_ArenaPlayer[1])
		return;

	auto Player = new ArenaPlayer{ ChiefName, Level, Country, Wins };
	if (!m_ArenaPlayer[0])
		m_ArenaPlayer[0] = Player;
	else
		m_ArenaPlayer[1] = Player;

	Update();
}

void CTArenaRegDlg::Update()
{
	m_pUWinCount->ShowComponent(TRUE);
	m_pULevel->ShowComponent(TRUE);
	m_pGroupUName->ShowComponent(TRUE);
	m_pGroupUC->ShowComponent(TRUE);
	m_pDWinCount->ShowComponent(TRUE);
	m_pDLevel->ShowComponent(TRUE);
	m_pGroupDName->ShowComponent(TRUE);
	m_pGroupDC->ShowComponent(TRUE);

	if (m_ArenaPlayer[0])
	{
		m_pUWinCount->m_strText.Format("Wins : %d", m_ArenaPlayer[0]->m_Wins);
		m_pULevel->m_strText.Format("Lv:%d", m_ArenaPlayer[0]->m_Level);
		m_pGroupUName->m_strText = m_ArenaPlayer[0]->m_ChiefName;
		m_pGroupUC->SetCurImage(m_ArenaPlayer[0]->m_Country);
	}
	else
	{
		m_pUWinCount->ShowComponent(FALSE);
		m_pULevel->ShowComponent(FALSE);
		m_pGroupUName->ShowComponent(FALSE);
		m_pGroupUC->ShowComponent(FALSE);
	}

	if (m_ArenaPlayer[1])
	{
		m_pDWinCount->m_strText.Format("Wins : %d", m_ArenaPlayer[1]->m_Wins);
		m_pDLevel->m_strText.Format("Lv:%d", m_ArenaPlayer[1]->m_Level);
		m_pGroupDName->m_strText = m_ArenaPlayer[1]->m_ChiefName;
		m_pGroupDC->SetCurImage(m_ArenaPlayer[1]->m_Country);
	}
	else
	{
		m_pDWinCount->ShowComponent(FALSE);
		m_pDLevel->ShowComponent(FALSE);
		m_pGroupDName->ShowComponent(FALSE);
		m_pGroupDC->ShowComponent(FALSE);
	}
}

void CTArenaRegDlg::AddPlayersQueue(CString strName)
{
	ArenaPlayer Player = { strName, 0, 0, 0 };
	m_Queue.push_back(Player);

	Update();
}

HRESULT CTArenaRegDlg::Render(DWORD dwTickCount)
{
	if(IsVisible())
	{
		for( INT i=0 ; i < 4 ; ++i )
		{
			if (m_Queue.size() > m_nListTop + i)
				m_pQUEUEName[i]->m_strText = m_Queue[m_nListTop + i].m_ChiefName;

			m_pComps[i]->ShowComponent(m_Queue.size() > i);
			m_pQUEUEName[i]->ShowComponent(m_Queue.size() > i);
		}
	}

	return CTClientUIBase::Render(dwTickCount);
}

void CTArenaRegDlg::Release()
{
	m_nListTop = 0;
	UpdateScrollPosition();

	for (BYTE i = 0; i < ARENA_MAX_TEAM; ++i)
		SAFE_DELETE(m_ArenaPlayer[i]);
	m_Queue.clear();

	m_pGroupDC->SetCurImage(-1);
	m_pGroupUC->SetCurImage(-1);

	m_pUWinCount->m_strText.Empty();
	m_pULevel->m_strText.Empty();
	m_pGroupUName->m_strText.Empty();

	m_pDWinCount->m_strText.Empty();
	m_pDLevel->m_strText.Empty();
	m_pGroupDName->m_strText.Empty();

	Update();
}

void CTArenaRegDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	TButton* Up = (TButton*) m_pTSCROLL->FindKid(40804);
	TButton* Scroll = (TButton*) m_pTSCROLL->FindKid(40824);
	TButton* Down = (TButton*)m_pTSCROLL->FindKid(40805);

	if (Up->HitTest(point))
		OnNotify((DWORD) this, TNM_LINEUP, nullptr);
	else if(Scroll->HitTest(point))
		OnNotify((DWORD) this, TNM_VSCROLL, nullptr);
	else if(Down->HitTest(point))
		OnNotify((DWORD) this, TNM_LINEDOWN, nullptr);

	if (FindKid(21)->HitTest(point))
		CTClientGame::GetInstance()->OnGM_CHALLENGE_JOIN();

	if (FindKid(1276)->HitTest(point))
		ShowComponent(FALSE);

	CTClientUIBase::OnLButtonDown(nFlags, point);
}

void CTArenaRegDlg::OnNotify(DWORD from, WORD msg, LPVOID param)
{
	switch(msg)
	{
	case TNM_LINEUP:
		{
			m_nListTop--;
			if(m_nListTop < 0) m_nListTop = 0;
		}

		break;

	case TNM_LINEDOWN:
		if (4 + m_nListTop < m_Queue.size())
			m_nListTop++;
		
		break;

	case TNM_VSCROLL:
		if( m_pTSCROLL && m_pTSCROLL->IsTypeOf( TCML_TYPE_SCROLL ) )
		{
			int nRange;
			int nPos;

			m_pTSCROLL->GetScrollPos( nRange, nPos);
			m_nListTop = nRange ? nPos : 0;
		}

		break;
	}

	UpdateScrollPosition();

	//CTClientUIBase::OnNotify( from, msg, param);
}

BOOL CTArenaRegDlg::DoMouseWheel( UINT nFlags, short zDelta, CPoint pt)
{
	BOOL bRESULT = FALSE;

	if(!CanProcess() || !m_pTSCROLL) 
		return FALSE;

	for( INT i=0 ; i < 5 ; ++i )
		if( HitTest( pt ) )
		{
			bRESULT = TRUE;
			break;
		}

	if(!bRESULT && m_pTSCROLL->HitTest( pt ))
		bRESULT = TRUE;

	if(!bRESULT)
		return bRESULT;

	int nRange, nPos;
	m_pTSCROLL->GetScrollPos( nRange, nPos);

	m_nListTop += zDelta>0? -1 : 1;
	m_nListTop = min(max(m_nListTop, 0), nRange);

	UpdateScrollPosition();

	return TRUE;
}

void CTArenaRegDlg::UpdateScrollPosition()
{
	if( m_pTSCROLL &&
	    m_pTSCROLL->IsTypeOf(TCML_TYPE_SCROLL))
	{
		if (m_Queue.size() < 4)
			m_pTSCROLL->SetScrollPos(0, 0);

		m_pTSCROLL->SetScrollPos( m_Queue.size() - 4, m_nListTop);
	}
}