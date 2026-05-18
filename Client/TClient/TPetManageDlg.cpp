#include "Stdafx.h"
#include "TPetManageDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClient.h"

#include "TCustomStrings.h"

#define TPET_ITEM_PER_PAGE		(5)
#define ID_SADDLE_TIME                (0x000065A3)
#define ID_SADDLE                      (0x000065A4)
#define ID_SADDLE_SPEED                (0x000065A5)
#define ID_SADDLE_NAME                 (0x000065A2)
//26022 - 26026
#define ID_PET_EXTEND_1                 (0x000065A6)
#define ID_PET_EXTEND_2                 (0x000065A7)
#define ID_PET_EXTEND_3                 (0x000065A8)
#define ID_PET_EXTEND_4                 (0x000065A9)
#define ID_PET_EXTEND_5                 (0x000065AA)

// ======================================================================
CTPetManageDlg::CTPetManageDlg(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc, CD3DDevice* pDevice)
	: CTPetDlg(pParent, pDesc, pDevice), m_nPrvSelIdx(T_INVALID)
{

	static DWORD picoviny[5] =
	{
		ID_PET_EXTEND_1,
			ID_PET_EXTEND_2,
			ID_PET_EXTEND_3,
			ID_PET_EXTEND_4,
			ID_PET_EXTEND_5
	};

	TButton* m_pEx[5];
	for(int i=0; i<5; i++)
	{
		m_pEx[i] = (TButton*) FindKid(picoviny[i]);
		m_pEx[i]->ShowComponent(FALSE);
		RemoveKid(m_pEx[i]);
	}


	CSize btnSize;
	CPoint btnPos;
	short yDiff = 0;
	short xDiff = 204;
	m_wSaddle = 0;

	m_pTSCROLL = static_cast<TScroll *>(FindKid(ID_CTRLINST_SCROLL));
	m_pRecallBtn = static_cast<TButton *>(FindKid(ID_CTRLINST_BTN_RECALL));
	m_pRecallBtn->ShowComponent(FALSE);
	m_pRecallBtn->m_menu[TNM_LCLICK] = GM_PET_RECALL;

	//Change Effect Button
	m_pCEffectBtnTpl = FindKid(ID_CTRLINST_BTN_RECALL);
	m_pCEffectBtnTpl->GetComponentSize(&btnSize);
	m_pCEffectBtnTpl->GetComponentPos(&btnPos);
	btnPos.SetPoint((btnPos.x - xDiff), (btnPos.y - yDiff));
	m_pCEffectBtn = new TButton(this, m_pCEffectBtnTpl->m_pDESC);
	m_pCEffectBtn->m_id = GetUniqueID();
	m_pCEffectBtn->SetComponentSize(btnSize);
	m_pCEffectBtn->m_bVisible = FALSE;
	AddKid(m_pCEffectBtn);
	m_pCEffectBtn->m_strText = TSTR_AF_BTN;
	m_pCEffectBtn->MoveComponent(btnPos);
	m_pCEffectBtn->ShowComponent(FALSE);
	m_pCEffectBtn->m_menu[TNM_LCLICK] = GM_PET_EFFECTCHANGE;

	//Delete Effect Button
	m_pDEffectBtnTpl = FindKid(ID_CTRLINST_BTN_RECALL);
	m_pDEffectBtnTpl->GetComponentSize(&btnSize);
	m_pDEffectBtnTpl->GetComponentPos(&btnPos);
	btnPos.SetPoint((btnPos.x - 100), (btnPos.y - yDiff));
	m_pDEffectBtn = new TButton(this, m_pDEffectBtnTpl->m_pDESC);
	m_pDEffectBtn->m_id = GetUniqueID();
	m_pDEffectBtn->SetComponentSize(btnSize);
	m_pDEffectBtn->m_bVisible = FALSE;
	AddKid(m_pDEffectBtn);
	m_pDEffectBtn->m_strText = TSTR_DF_BTN;
	m_pDEffectBtn->MoveComponent(btnPos);
	m_pDEffectBtn->ShowComponent(FALSE);

	m_pDEffectBtn->m_menu[TNM_LCLICK] = GM_PET_EFFECTDELETE;


	m_pTSCROLL->SetScrollType(TRUE);
	m_nListTop = 0;

	static DWORD dwNAME[TPET_ITEM_PER_PAGE] =
	{
		ID_CTRLINST_NAME1,
		ID_CTRLINST_NAME2,
		ID_CTRLINST_NAME3,
		ID_CTRLINST_NAME4,
		ID_CTRLINST_NAME5,
	};
/*
	static DWORD dwSTATE[TPET_ITEM_PER_PAGE] =
	{
		ID_CTRLINST_STATE1,
		ID_CTRLINST_STATE2,
		ID_CTRLINST_STATE3,
		ID_CTRLINST_STATE4,
		ID_CTRLINST_STATE5,
	};
*/
	static DWORD dwPERIOD[TPET_ITEM_PER_PAGE] =
	{
		ID_CTRLINST_PERIOD1,
		ID_CTRLINST_PERIOD2,
		ID_CTRLINST_PERIOD3,
		ID_CTRLINST_PERIOD4,
		ID_CTRLINST_PERIOD5,
	};

	static DWORD dwICON[TPET_ITEM_PER_PAGE] = 
	{
		ID_CTRLINST_ICON1,
		ID_CTRLINST_ICON2,
		ID_CTRLINST_ICON3,
		ID_CTRLINST_ICON4,
		ID_CTRLINST_ICON5,
	};

	static DWORD dwHOVER[TPET_ITEM_PER_PAGE] =
	{
		ID_CTRLINST_HOVER1,
		ID_CTRLINST_HOVER2,
		ID_CTRLINST_HOVER3,
		ID_CTRLINST_HOVER4,
		ID_CTRLINST_HOVER5,
	};

	static DWORD dwSEL[TPET_ITEM_PER_PAGE] =
	{
		ID_CTRLINST_SEL1,
		ID_CTRLINST_SEL2,
		ID_CTRLINST_SEL3,
		ID_CTRLINST_SEL4,
		ID_CTRLINST_SEL5,
	};

	for( INT i=0 ; i < TPET_ITEM_PER_PAGE ; ++i )
	{
		m_pTNAME[i] = FindKid( dwNAME[i] );
//		m_pTSTATE[i] = FindKid( dwSTATE[i] );
		m_pTPERIOD[i] = FindKid( dwPERIOD[i] );
		m_pTICON[i] = (TImageList*) FindKid( dwICON[i] );
		m_pTHOVER[i] = FindKid( dwHOVER[i] );
		m_pTSEL[i] = FindKid( dwSEL[i] );

		m_pTNAME[i]->m_strText.Empty();
	//	m_pTSTATE[i]->m_strText.Empty();
		m_pTPERIOD[i]->m_strText.Empty();
		m_pTICON[i]->SetCurImage(0);
		m_pTHOVER[i]->ShowComponent(FALSE);
	}


	m_pTime = static_cast<TComponent*>(FindKid(ID_SADDLE_TIME));
	m_pSaddle = static_cast<TImageList*>(FindKid(ID_SADDLE));
	m_pSpeed = static_cast<TComponent*>(FindKid(ID_SADDLE_SPEED));
	m_pSaddleName = static_cast<TComponent*>(FindKid(ID_SADDLE_NAME));

	m_pSaddleName->m_strText = "Saddle";
	m_pTime->m_strText.Empty();
	m_pSpeed->m_strText.Empty();
	m_pSaddle->SetCurImage(T_INVALID);

	m_wSaddle = 0;

	ClearPet();
}
// ----------------------------------------------------------------------
CTPetManageDlg::~CTPetManageDlg()
{
	ClearPet();
}
BYTE CTPetManageDlg::GetSaddleSpeed(WORD m_wSaddleID)
{
	LPTITEM pSADDLE;

	if(m_wSaddleID)
	{
		pSADDLE = CTChart::FindTITEMTEMP( m_wSaddleID );

		if(!pSADDLE)
			return 176;

		switch(pSADDLE->m_wUseValue)
		{
		case 26:
			return 200;
			break;
		default : 
			return 176;
			break;
		}
	}
	return 176;
}
CString	CTPetManageDlg::GetEffectString(BYTE bEffect)
{
	switch(bEffect)
	{
	case 1:
		return	"Darkblue";
	case 2:
		return	"Lava";
	case 3:
		return	"Lightning";
	case 4:
		return	"Ice";
	case 5:
		return	"Blackgreen";
	case 6:
		return	"Pink";
	case 7:
		return	"Stormblue";
	case 8:
		return	"Naturegreen";
	case 9:
		return	"Magma";
	case 10:
		return	"Neon Green";
	case 11:
		return	"Magic Ice";
	case 12:
		return	"Magic Blue";
	case 13:
		return	"Shiny Pink";
	case 14:
		return	"Magic Pink";
	case 15:
		return	"Gold";
	case 16:
		return "Abyss";
	}

	return "";
}

// ======================================================================
void CTPetManageDlg::UpdateScrollPosition()
{
	if( m_pTSCROLL &&
	    m_pTSCROLL->IsTypeOf(TCML_TYPE_SCROLL))
	{
		int nRange = m_vPetArray.size()-TPET_ITEM_PER_PAGE;
		if(nRange < 0)
			nRange = 0;

		m_pTSCROLL->SetScrollPos( nRange, m_nListTop);
	}
}
// ======================================================================
void CTPetManageDlg::AddPet(const CString& strName, WORD wPetID, const CTime& tEndTime, BYTE m_bEffect)
{
	CTClientPet* pPet = NewDisplayPet(strName,wPetID,tEndTime, m_bEffect);
	m_vPetArray.push_back(pPet);
	UpdateScrollPosition();
}
// ----------------------------------------------------------------------
void CTPetManageDlg::SetPet(INT nIdx, const CString& strName, WORD wPetID, const CTime& tEndTime, BYTE m_bEffect)
{
	CTClientPet* pPet = GetPet(nIdx);

    pPet->SetPetInfo(strName,wPetID,tEndTime, m_bEffect);

	UpdatePetInfo(nIdx);
	NotifyUpdate();
}

void CTPetManageDlg::SetSaddle(WORD m_wSaddle, const CTime& tEndTime, BYTE m_bType)
{
	CString m_strSpeed = "";
	CString strPERIOD = "";
	this->m_wSaddle = m_wSaddle;
	if (m_wSaddle) 
	{
		LPTITEMVISUAL pSADDLEV;
		LPTITEM pSADDLE = CTChart::FindTITEMTEMP(m_wSaddle);

		if (m_wSaddle && pSADDLE)
			pSADDLEV = CTChart::FindTITEMVISUAL(pSADDLE->m_wVisual[0]);

		m_strSpeed.Format("Speed : %d%%", GetSaddleSpeed(m_wSaddle));

		if (pSADDLE && pSADDLEV)
		{
			m_pSaddleName->m_strText = pSADDLE->m_strNAME;
			m_pSaddle->SetCurImage(pSADDLEV->m_wIcon);
		}

		CTime dDate(tEndTime);

		if (m_bType)
			strPERIOD.Empty();
		else if (dDate > CTClientApp::m_dCurDate)
		{
			CTimeSpan timeSpan = dDate - CTClientApp::m_dCurDate;

			if (timeSpan.GetDays() > 0)
				strPERIOD = CTChart::Format(TSTR_FMT_PET_EXTPERIOD, timeSpan.GetDays());
			else
			{
				if (timeSpan.GetHours() > 0)
					strPERIOD = CTChart::Format(TSTR_FMT_PET_EXTPERIOD_HOUR_MIN, timeSpan.GetHours(), timeSpan.GetMinutes());
				else
					strPERIOD = CTChart::Format(TSTR_FMT_PET_EXTPERIOD_MIN, timeSpan.GetMinutes());
			}
		}
		else
			strPERIOD.Empty();
	}
	else
	{
		m_pSaddleName->m_strText = "Saddle";
		m_pSaddle->SetCurImage(T_INVALID);
	}

	m_pTime->m_strText = strPERIOD;
	m_pSpeed->m_strText = m_strSpeed;
}
// ======================================================================
void CTPetManageDlg::UpdatePetInfo(INT nIdx)
{
	CTClientPet* pPet = GetPet(nIdx);
	BOOL bPetRec = IsPetRecall(pPet);

	if( m_nListSelect == nIdx )
	{
		if( bPetRec )
		{
			m_pRecallBtn->m_strText = CTChart::LoadString( TSTR_FMT_PETMNGBTN_CANCELRECALL);
		}
		else
		{
			m_pRecallBtn->m_strText = CTChart::LoadString( TSTR_FMT_PETMNGBTN_DORECALL);
		}
	}
}
// ======================================================================
void CTPetManageDlg::RemovePet( WORD wPetID )
{
	ClientPetArray::iterator it, end;
	it = m_vPetArray.begin();
	end = m_vPetArray.end();

	for(; it != end ; ++it )
	{
		if( (*it)->GetPetID() == wPetID )
		{
			delete (*it);
			m_vPetArray.erase( it );
			break;
		}
	}
	
	m_nListTop = 0;
	m_nListSelect = -1;
	m_pDisplayPet = NULL;
	m_nPrvSelIdx = T_INVALID;
	m_pKindNameTxt->m_strText.Empty();

	UpdateScrollPosition();
	NotifyUpdate();
}
// ----------------------------------------------------------------------
void CTPetManageDlg::ClearPet()
{
	INT nCnt = GetPetCount();
	for(INT i=0; i<nCnt; ++i)
		delete m_vPetArray[i];

	m_vPetArray.clear();
	m_nPrvSelIdx = T_INVALID;
	m_pDisplayPet = NULL;
	m_pRecallBtn->ShowComponent(FALSE);
	m_nListTop = 0;
	m_nListSelect = -1;

	for( INT i=0 ; i < TPET_ITEM_PER_PAGE ; ++i )
	{
		m_pTNAME[i]->m_strText.Empty();
	//	m_pTSTATE[i]->m_strText.Empty();
		m_pTPERIOD[i]->m_strText.Empty();
		m_pTICON[i]->SetCurImage(0);
		m_pTHOVER[i]->ShowComponent(FALSE);
	}

	UpdateScrollPosition();
}
// ======================================================================
INT CTPetManageDlg::GetPetCount() const
{
	return (INT)m_vPetArray.size();
}
// ----------------------------------------------------------------------
BOOL CTPetManageDlg::IsPetEmpty() const
{
	return m_vPetArray.empty();
}
// ======================================================================
CTClientPet* CTPetManageDlg::GetPet(INT nIdx) const
{
	if( !m_vPetArray.empty() && 
		nIdx != T_INVALID &&
		nIdx < m_vPetArray.size() )
		return const_cast<CTClientPet*>(m_vPetArray[nIdx]);
	else
		return NULL;
}
// ----------------------------------------------------------------------
CTClientPet* CTPetManageDlg::GetSelectPet() const
{
	INT nIdx = GetSelectIdx();
	if( nIdx != T_INVALID )
		return m_vPetArray[nIdx];

	return NULL;
}
// ----------------------------------------------------------------------
INT CTPetManageDlg::GetSelectIdx() const
{
	return m_nListSelect >= 0 ? m_nListSelect : T_INVALID;
}
// ----------------------------------------------------------------------
INT CTPetManageDlg::FindPetByID(WORD wPetID) const
{
	INT nCnt = GetPetCount();
	for(INT i=0; i<nCnt; ++i)
	{
		if( m_vPetArray[i]->GetPetID() == wPetID )
			return i;
	}

	return T_INVALID;
}
// ----------------------------------------------------------------------
INT CTPetManageDlg::FindPetByName(const CString& strName) const
{
	INT nCnt = GetPetCount();
	for(INT i=0; i<nCnt; ++i)
	{
		if( m_vPetArray[i]->GetPetKindName() == strName )
			return i;
	}

	return T_INVALID;
}
// ======================================================================
BOOL CTPetManageDlg::IsPetRecall(CTClientPet* pPet)
{
	CTClientPet* pMainPet = CTClientGame::GetInstance()->GetMainPet();
	if( pMainPet && pMainPet->GetPetID() == pPet->GetPetID() )
		return TRUE;

	return FALSE;
}
// ======================================================================
void CTPetManageDlg::SetRecalling(BOOL bRecalling)
{
	m_bRecalling = bRecalling;
	NotifyUpdate();
}
// ======================================================================

// ======================================================================
void CTPetManageDlg::Update(DWORD dwTickCount)
{
	INT nSelId = GetSelectIdx();
	m_pDisplayPet = GetPet(nSelId);
	if( nSelId != T_INVALID && m_pDisplayPet)
	{
		if( m_bRecalling )
			m_pRecallBtn->m_strText = CTChart::LoadString( TSTR_FMT_PETMNGBTN_CANCELRECALL);
		else if( CTClientGame::GetInstance()->IsPetRecalled( m_pDisplayPet->GetPetID() ) )
			m_pRecallBtn->m_strText = CTChart::LoadString( TSTR_FMT_PETMNGBTN_CANCELRECALL);
		else if( m_pDisplayPet->GetPetEndTime() != CTime(0) &&
				CTime(m_pDisplayPet->GetPetEndTime()) < CTClientApp::m_dCurDate )
			m_pRecallBtn->m_strText = CTChart::LoadString( TSTR_DELETE);
		else
			m_pRecallBtn->m_strText = CTChart::LoadString( TSTR_FMT_PETMNGBTN_DORECALL);

		m_pRecallBtn->ShowComponent(TRUE);
	}
	else
		m_pRecallBtn->ShowComponent(FALSE);

	CTPetDlg::Update(dwTickCount);
}
// ======================================================================
void CTPetManageDlg::ShowComponent(BOOL bVisible)
{
	if( m_bVisible == bVisible )
		return;

	CTPetDlg::ShowComponent(bVisible);

	if( bVisible )
	{
		if( GetSelectIdx() == T_INVALID )
		{
			if( IsPetEmpty() )
			{
				m_pKindNameTxt->m_strText.Empty();
				m_pRecallBtn->ShowComponent(FALSE);

				for( INT i=0 ; i < TPET_ITEM_PER_PAGE ; ++i )
				{
					m_pTNAME[i]->m_strText.Empty();
					//m_pTSTATE[i]->m_strText.Empty();
					m_pTPERIOD[i]->m_strText.Empty();
					m_pTICON[i]->SetCurImage(0);
					m_pTHOVER[i]->ShowComponent(FALSE);
				}
			}
			else
			{
				m_nListSelect = 0;
				NotifyUpdate();
			}
		}
	}
}
// ======================================================================
HRESULT CTPetManageDlg::Render(DWORD dwTickCount)
{
	if( !m_bVisible )
		return S_OK;

	if (CTClientGame::IsInBOWMap() || CTClientGame::IsInBRMap())
	{
		m_pCEffectBtn->ShowComponent(FALSE);
		m_pDEffectBtn->ShowComponent(FALSE);
	}

	if( !IsPetEmpty() )
	{
		INT nSelId = GetSelectIdx();
		if( m_nPrvSelIdx != nSelId )
		{
			m_pDisplayPet = NULL;
			m_nPrvSelIdx = nSelId;

			NotifyUpdate();
		}

		for( INT i=0 ; i < TPET_ITEM_PER_PAGE ; ++i )
		{
			if( m_nListTop+i >= m_vPetArray.size() )
			{
				m_pTNAME[i]->m_strText.Empty();
			//	m_pTSTATE[i]->m_strText.Empty();
				m_pTPERIOD[i]->m_strText.Empty();
				m_pTICON[i]->SetCurImage(0);
				m_pTHOVER[i]->ShowComponent(FALSE);
				continue;
			}

			CTClientPet* pPET = m_vPetArray[ m_nListTop+i ];
			CTClientPet* pMount = GetSelectPet();

			CString strColor = "";
			strColor = GetEffectString(pPET->GetPetEffect());

			if(pPET->GetPetEffect() == 0)
					m_pTNAME[i]->m_strText = pPET->GetName();
			else
				m_pTNAME[i]->m_strText = pPET->GetName() + " | " + strColor;

			if(pMount)
			{
				if(pMount->GetPetEffect() == 0)
				{
					m_pCEffectBtn->m_strText = TSTR_AF_BTN;
					m_pDEffectBtn->ShowComponent(FALSE);
					m_pDEffectBtn->EnableComponent(FALSE);
				}
				else
				{
					m_pCEffectBtn->m_strText = TSTR_CF_BTN;
					m_pDEffectBtn->ShowComponent(TRUE);
					m_pDEffectBtn->EnableComponent(TRUE);
				}
			}
			CString strPERIOD;

			CTime dDate(pPET->GetPetEndTime());
			if( dDate == CTime(0) )
			{
				strPERIOD.Empty();
			}
			else if( dDate > CTClientApp::m_dCurDate)
			{
				CTimeSpan timeSpan = dDate - CTClientApp::m_dCurDate;

				if( timeSpan.GetDays() > 0 )
					strPERIOD = CTChart::Format( TSTR_FMT_PET_EXTPERIOD, timeSpan.GetDays() );
				else
				{
					if( timeSpan.GetHours() > 0 )
						strPERIOD = CTChart::Format( TSTR_FMT_PET_EXTPERIOD_HOUR_MIN, timeSpan.GetHours(), timeSpan.GetMinutes() );
					else
						strPERIOD = CTChart::Format( TSTR_FMT_PET_EXTPERIOD_MIN, timeSpan.GetMinutes() );
				}
			}
			else
				strPERIOD = CTChart::LoadString( TSTR_FMT_PET_PERIOD_FIRED);

			m_pTPERIOD[i]->m_strText = strPERIOD;

			LPTPET pTPET = CTChart::FindTPETTEMP( pPET->GetPetID() );
			if( pTPET )
				m_pTICON[i]->SetCurImage( (int) pTPET->m_wIcon );
			else
				m_pTICON[i]->SetCurImage(0);

			if( m_nListTop+i == m_nListSelect )
				m_pTHOVER[i]->ShowComponent( TRUE);
			else
				m_pTHOVER[i]->ShowComponent( FALSE);
		}
	}
	else
	{
		m_pKindNameTxt->m_strText.Empty();
		
		for( INT i=0 ; i < TPET_ITEM_PER_PAGE ; ++i )
		{
			m_pTNAME[i]->m_strText.Empty();
	//		m_pTSTATE[i]->m_strText.Empty();
			m_pTPERIOD[i]->m_strText.Empty();
			m_pTICON[i]->SetCurImage(0);
			m_pTHOVER[i]->ShowComponent(FALSE);
		}
	}

	return CTPetDlg::Render(dwTickCount);
}
// ======================================================================
void CTPetManageDlg::OnLButtonUp( UINT nFlags, CPoint pt )
{
	for( INT i=0 ; i < TPET_ITEM_PER_PAGE ; ++i )
		if( m_pTSEL[i]->HitTest( pt ) && m_vPetArray.size() > m_nListTop+i )
			m_nListSelect = m_nListTop+i;

	CTPetDlg::OnLButtonUp( nFlags, pt);
}
// ======================================================================
BYTE CTPetManageDlg::OnBeginDrag( LPTDRAG pDRAG, CPoint point)
{
	for( INT i=0 ; i < TPET_ITEM_PER_PAGE ; ++i )
	{
		if( m_pTICON[i]->GetCurImage() > 0 &&
			m_pTICON[i]->HitTest(point) )
		{
			CTClientPet* pPET = GetPet(m_nListTop+i);
			if( pDRAG && pPET)
			{
				CPoint pt;

				pDRAG->m_pIMAGE = new TImageList(
					NULL,
					*m_pTICON[i]);

				pDRAG->m_pIMAGE->SetCurImage(m_pTICON[i]->GetCurImage());
				pDRAG->m_pIMAGE->m_strText = m_pTICON[i]->m_strText;
				BYTE bPetID = (BYTE) pPET->GetPetID();
				pDRAG->m_dwParam = bPetID;

				m_pTICON[i]->GetComponentPos(&pt);
				m_pTICON[i]->ComponentToScreen(&pt);
				m_pTICON[i]->m_strText.Empty();

				pDRAG->m_pIMAGE->ShowComponent(TRUE);
				pDRAG->m_pIMAGE->MoveComponent(pt);
			}

			return TRUE;
		}
	}
	if( m_pSaddle->GetCurImage() > 0 &&
		m_pSaddle->HitTest(point) )
	{
		if( pDRAG )
		{
			CPoint pt;

			pDRAG->m_pIMAGE = new TImageList(
				NULL,
				*m_pSaddle);

			pDRAG->m_pIMAGE->SetCurImage(m_pSaddle->GetCurImage());
			pDRAG->m_pIMAGE->m_strText = m_pSaddle->m_strText;;
			pDRAG->m_dwParam = m_wSaddle;

			m_pSaddle->GetComponentPos(&pt);
			m_pSaddle->ComponentToScreen(&pt);
			m_pSaddle->m_strText.Empty();

			pDRAG->m_pIMAGE->ShowComponent(TRUE);
			pDRAG->m_pIMAGE->MoveComponent(pt);
		}

		return TRUE;
	}
	return FALSE;
}

void CTPetManageDlg::ResetRace( BYTE bRaceID )
{
	CTClientPet* m_pLASTPET = NULL;
	INT m_nLastSelect = m_nListSelect;
		
	ShowComponent(FALSE);

	m_pDisplayPet = NULL;
	m_nListSelect = -1;

	std::vector<CString> vNames;
	std::vector<WORD> vPetIDs;
	std::vector<CTime> vEndTimes;
	std::vector<BYTE> vEffects;

	ClientPetArray::iterator itPET, endPET;
	itPET = m_vPetArray.begin();
	endPET = m_vPetArray.end();
	for(; itPET != endPET ; ++itPET )
	{
		CString strNAME = (*itPET)->GetPetName();
		BYTE m_bEffect = (*itPET)->GetPetEffect();
		WORD wPetID = (*itPET)->GetPetID();
		CTime tEndTime = (*itPET)->GetPetEndTime();

		vNames.push_back( strNAME );
		vPetIDs.push_back( wPetID );
		vEndTimes.push_back( tEndTime );
		vEffects.push_back(m_bEffect);

		delete (*itPET);
	}

	m_vPetArray.clear();

	INT nCount = (INT) vNames.size();
	for( INT i=0 ; i < nCount ; ++i )
	{
		WORD wPetID = vPetIDs[i];

		m_vPetArray.push_back(
			NewDisplayPet( vNames[i], wPetID, vEndTimes[i], vEffects[i]));
	}

	m_nListSelect = m_nLastSelect;
	m_pDisplayPet = GetPet(m_nListSelect);

	UpdateScrollPosition();

}

void CTPetManageDlg::OnNotify(DWORD from, WORD msg, LPVOID param)
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
		if(TPET_ITEM_PER_PAGE < m_vPetArray.size()-m_nListTop)
		{
			m_nListTop ++;
			if(m_nListTop > m_vPetArray.size()-TPET_ITEM_PER_PAGE) 
				m_nListTop = m_vPetArray.size()- TPET_ITEM_PER_PAGE;
		}

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

	if( m_nListSelect < m_nListTop )
		m_nListSelect = m_nListTop;
	else
	{
		int nLAST = min( INT(m_vPetArray.size()), m_nListTop + TPET_ITEM_PER_PAGE) - 1;

		if( m_nListSelect > nLAST )
			m_nListSelect = nLAST;
	}

	CTPetDlg::OnNotify( from, msg, param);
}

void CTPetManageDlg::OnKeyDown(UINT nChar, int nRepCnt, UINT nFlags)
{
	if(!CanProcess()) return;

	switch(nChar)
	{
	case VK_LEFT:
	case VK_UP:			SelectUp(1);				break;
	case VK_RIGHT:
	case VK_DOWN:		SelectDown(1);				break;
	case VK_PRIOR:		SelectUp(TPET_ITEM_PER_PAGE);	break;
	case VK_NEXT:		SelectDown(TPET_ITEM_PER_PAGE); break;
	}
}

BOOL CTPetManageDlg::DoMouseWheel( UINT nFlags, short zDelta, CPoint pt)
{
	BOOL bRESULT = FALSE;

	if(!CanProcess() || !m_pTSCROLL) 
		return FALSE;

	for( INT i=0 ; i < TPET_ITEM_PER_PAGE ; ++i )
		if( m_pTSEL[i]->HitTest( pt ) )
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

	if( m_nListSelect < m_nListTop )
		m_nListSelect = m_nListTop;
	else
	{
		int nLAST = min( INT(m_vPetArray.size()), m_nListTop + TPET_ITEM_PER_PAGE) - 1;

		if( m_nListSelect > nLAST )
			m_nListSelect = nLAST;
	}
	UpdateScrollPosition();

	return TRUE;
}

void CTPetManageDlg::SelectUp(int nLine)
{
	int nTarget = m_nListSelect - nLine;
	if(nTarget <0)
		nTarget = (nTarget<0 && m_vPetArray.size()>0)? 0: -1;

	if(nTarget >=0 && nTarget < m_nListTop)
		m_nListTop = nTarget;

	SetCurSelItem(nTarget);
	UpdateScrollPosition();
}

void CTPetManageDlg::SelectDown(int nLine)
{
	int nTarget = (m_nListSelect < 0)? 0: m_nListSelect + nLine;

	if(nTarget >= m_vPetArray.size())
		nTarget = m_vPetArray.size() -1;

	if(nTarget>=0)
	{
		if(nTarget < m_nListTop)
			m_nListTop = m_nListSelect;
		else if(abs(nTarget - m_nListTop) >= TPET_ITEM_PER_PAGE)
			m_nListTop = m_nListTop + nLine;
	}

	if(SetCurSelItem(nTarget))
		UpdateScrollPosition();
}

BYTE CTPetManageDlg::SetCurSelItem( int nLine)
{
	m_nListSelect = nLine;

	if( m_nListSelect < 0 )
		return FALSE;

	if( m_nListSelect < m_nListTop )
	{
		m_nListTop = m_nListSelect;
	}
	else if( m_nListSelect + 1 >= m_nListTop + TPET_ITEM_PER_PAGE )
	{
		m_nListTop += m_nListSelect - ( m_nListTop + TPET_ITEM_PER_PAGE ) + 1;

		if( m_nListTop > m_vPetArray.size() - TPET_ITEM_PER_PAGE ) 
			m_nListTop = m_vPetArray.size() - TPET_ITEM_PER_PAGE;
	}

	return TRUE;
}
