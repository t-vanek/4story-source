#include "StdAfx.h"
#include "TBoxOpen.h"
#include "TClientGame.h"
#include "TBoxOpenList.h"
static DWORD m_dwDefaultC;

CTBoxOpenDlg::CTBoxOpenDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
	: CTClientUIBase(pParent, pDesc)
{
	m_pItemName = FindKid(26953);
	m_pKey = FindKid(26773);
	m_pEnd = FindKid(26962);
	m_pUseless = FindKid(26963);
	m_pUseless2 = FindKid(26964);
	m_pUseless3 = FindKid(26926);

	m_pOpen = (TButton*) FindKid(21);
	m_pPreview = (TButton*) FindKid(875);

	m_pKeyImage = (TImageList*) FindKid(11106);
	m_pReceived = (TImageList*) FindKid(26952);

	m_pProgress = (TGauge*) FindKid(26925);
	m_pProgress->SetStyle(TGS_GROW_RIGHT);

	TComponent* m_pTest = FindKid(26965);
	m_pKeyImage->m_strText.Empty();

	m_pOpen->m_menu[TNM_LCLICK] = GM_OPEN_BOX;
	m_pPreview->m_menu[TNM_LCLICK] = GM_TOGGLE_LIST;

	m_bUnboxing = FALSE;
	m_bShowBox = TRUE;
	m_bShowList = TRUE;

	m_dwDefaultC = m_pOpen->m_pFont->m_dwColor;

	RemoveKid(m_pTest);
}

CTBoxOpenDlg::~CTBoxOpenDlg()
{

}
void CTBoxOpenDlg::Release()
{
	m_pReceived->SetCurImage(T_INVALID);
	
	m_pItemName->m_strText.Empty();
	m_bUnboxing = FALSE;
	m_dwTotal = 0;
	m_pReceived->m_strText.Empty();
}

void CTBoxOpenDlg::SetSession(WORD m_wBoxID, BYTE m_bInvenID, BYTE m_bSlotID)
{
	CTClientGame* pGame = CTClientGame::GetInstance();
	CTBoxOpenListDlg* m_pList = static_cast<CTBoxOpenListDlg*>(pGame->FindFrame(TFRAME_OPENBOXLIST));

	Release();
	m_pProgress->SetGauge(0, 0, FALSE);
	m_pOpen->EnableComponent(TRUE);

	m_bShowBox = TRUE;
	m_bShowList = TRUE;
	LPTITEM pITEM = CTChart::FindTITEMTEMP(m_wBoxID);
	LPTITEMVISUAL pITEMV;
	if(pITEM)
		pITEMV = CTChart::FindTITEMVISUAL(pITEM->m_wVisual[0]);
	if(pITEMV)
		m_pKeyImage->SetCurImage(pITEMV->m_wIcon);

	ShowComponent(TRUE);
}

BOOL CTBoxOpenDlg::Update(DWORD dwTickCount)
{
	m_dwTotal += dwTickCount;
	if(m_bUnboxing)
	{
		if( m_dwTotal >= 1000 )
		{
			m_pProgress->SetGauge(100, 100, FALSE);
			return TRUE;
		}
		else
		{
			m_pProgress->SetGauge(m_dwTotal, 1000, FALSE);
			return FALSE;
		}
	}

	return FALSE;
}

HRESULT CTBoxOpenDlg::Render(DWORD dwTickCount)
{
	if(IsVisible())
	{
		m_pUseless2->ShowComponent(FALSE);
		m_pUseless3->ShowComponent(FALSE);

		m_pProgress->ShowComponent(!m_bShowBox);
		CTClientItem* pTItem = CTClientGame::GetInstance()->FindMainItemByInven(m_bInvenID,m_bSlotID);

		if( !pTItem || !pTItem->GetCount() )
		{
			m_pOpen->m_strText = "Close";
			m_pOpen->m_menu[TNM_LCLICK] = GM_CLOSE_BOX;
		}
		else

		{
			m_pOpen->m_strText = "Open";
			m_pOpen->m_menu[TNM_LCLICK] = GM_OPEN_BOX;
		}

		if(!m_pOpen->IsEnable())
			m_pOpen->m_pFont->m_dwColor = 0xFF696969; //XD

		else
			m_pOpen->m_pFont->m_dwColor = m_dwDefaultC;
	}

	return CTClientUIBase::Render(dwTickCount);
}

void CTBoxOpenDlg::Open()
{
	Release();
	m_dwTotal = 0;
	m_bUnboxing = TRUE;
	m_bShowBox = FALSE;
}

void CTBoxOpenDlg::SwitchJirkus()
{
	RemoveKid(m_pProgress);
	AddKid(m_pProgress);

	RemoveKid(m_pItemName);
	AddKid(m_pItemName);

	RemoveKid(m_pReceived);
	AddKid(m_pReceived);

}
void CTBoxOpenDlg::SetReward(WORD m_wItemID, BYTE bCount, CString strCustom)
{
	if(strCustom == NAME_NULL)
	{
		LPTITEM pITEM = CTChart::FindTITEMTEMP(m_wItemID);
		LPTITEMVISUAL pITEMV;
		if(pITEM)
			pITEMV = CTChart::FindTITEMVISUAL(pITEM->m_wVisual[0]);
		if(pITEMV)
		{
			m_pItemName->m_strText = pITEM->m_strNAME;
			m_pReceived->SetCurImage(pITEMV->m_wIcon);
			if(bCount > 1)
				m_pReceived->m_strText.Format("%d", bCount);
			else
				m_pReceived->m_strText.Empty();
		}
	}
	else
	{
		m_pItemName->m_strText = strCustom;
		m_pReceived->SetCurImage(T_INVALID);
		m_pReceived->m_strText.Empty();
	}

	m_bUnboxing = FALSE;
}