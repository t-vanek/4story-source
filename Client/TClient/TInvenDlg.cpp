#include "StdAfx.h"
#include "TClientGame.h"
#include "TClient.h"
#include "Resource.h"
#include "TCharInfoNewInner.h"

TFRAMEOFFSET CTInvenDlg::m_vSubInvenPos[ MAX_SUBINVEN ] =
{
	TFRAMEOFFSET( TFRAME_INVEN_1, 0, CPoint(0,0) ),
	TFRAMEOFFSET( TFRAME_INVEN_2, 0, CPoint(0,0) ),
	TFRAMEOFFSET( TFRAME_INVEN_3, 0, CPoint(0,0) ),
	TFRAMEOFFSET( TFRAME_INVEN_4, 0, CPoint(0,0) ),
	TFRAMEOFFSET( TFRAME_INVEN_5, 0, CPoint(0,0) )
};

TFRAMEOFFSET CTInvenDlg::m_pDefInven = TFRAMEOFFSET(TFRAME_DEFINVEN, 0, CPoint(0,0));

CTInvenDlg::CTInvenDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, WORD wFrameID)
	:CTClientUIBase( pParent, pDesc),
m_pSubInvenPos( NULL )
{
	m_id = MAKELONG( WORD(m_id), wFrameID);
	m_pTINVEN = NULL;
	// 기간연장용 가방 컴포넌트들
	m_pEXTPERIOD = FindKid( ID_CTRLINST_EXT_TEXT);
	TComponent* pBTN = FindKid( ID_CTRLINST_EXT_BTN);

	if(pBTN)
		m_EXTINVENPERIOD_Button = pBTN->m_id;
	else
        m_EXTINVENPERIOD_Button = 0;

	m_bCharInfo = FALSE;
	switch(wFrameID)
	{
	case TFRAME_DEFINVEN:
		{
			CPoint vPOS = CTClientUIBase::GetUIBasisPos(
				CTInvenDlg::m_pDefInven.m_bBasisPoint,
				CTInvenDlg::m_pDefInven.m_vOffset);

			MoveComponent(vPOS);		
		}
		break;
	case 1206:
		{
			CPoint pt = CTClientUIBase::m_vBasis[TBASISPOINT_RIGHT_BOTTOM];
			pt.x -= 280;
			pt.y -= 365;
			MoveComponent(pt);
		}
		break;
	case 1135:
		m_bCharInfo = TRUE;


		break;
	}

	for (INT i = 0; i < MAX_SUBINVEN; ++i)
	{
		if (CTInvenDlg::m_vSubInvenPos[i].m_dwFRAME == wFrameID)
		{
			m_pSubInvenPos = &CTInvenDlg::m_vSubInvenPos[i];

			CPoint vPOS = CTClientUIBase::GetUIBasisPos(
				CTInvenDlg::m_vSubInvenPos[i].m_bBasisPoint,
				CTInvenDlg::m_vSubInvenPos[i].m_vOffset );

			MoveComponent(vPOS);
		}
	}
}

CTInvenDlg::~CTInvenDlg()
{
}

HRESULT CTInvenDlg::Render( DWORD dwTickCount)
{
	if(IsVisible())
	{
		for( BYTE i=0; i<TINVENSLOT_COUNT; i++)
		{
			TGauge *pTGauge = (TGauge *) FindKid(CTClientGame::m_vTICONGAUGE[i]);

			if(pTGauge)
			{
				CTClientItem *pTITEM = m_pTINVEN ? m_pTINVEN->FindTItem(i) : NULL;



				if (pTITEM)
				{


					DWORD dwTick = pTITEM && pTITEM->GetTITEM() ? CTClientItem::GetTick(pTITEM->GetTITEM()->m_wDelayGroupID) : 0;

					if(dwTick)
					{
						pTGauge->SetStyle( TGS_GROW_UP );
						pTGauge->m_strText = CTClientGame::ToTimeString(dwTick); 
						pTGauge->SetGauge( dwTick, pTITEM->GetTITEM()->m_dwDelay);

						pTGauge->ShowComponent(TRUE);
					}
					else
						pTGauge->ShowComponent(FALSE);
				}
				else
					pTGauge->ShowComponent(FALSE);
			}
		}

		if(m_pEXTPERIOD)
		{
			DWORD dwDays = 0;

			CTime dDate(m_pTINVEN->m_dEndTime);

			if( dDate > CTClientApp::m_dCurDate)
			{
				CTimeSpan timeSpan = dDate-CTClientApp::m_dCurDate;
				dwDays = (DWORD) timeSpan.GetDays();
				if( dwDays == 0 &&
					timeSpan.GetHours() != 0 )
					dwDays = 1;


				m_pEXTPERIOD->m_strText = CTChart::Format( TSTR_FMT_EXTPERIOD, dwDays);
			}
			else
				m_pEXTPERIOD->m_strText = "Unlimited";
		}
	}

	return CTClientUIBase::Render(dwTickCount);
}

void CTInvenDlg::OnNotify(DWORD from, WORD msg, LPVOID param)
{
	if( from == m_EXTINVENPERIOD_Button &&
		msg == TNM_LCLICK &&
		m_pTINVEN)
	{
		DWORD dwPARAM = MAKELONG(
			(WORD)( m_pTINVEN->m_bInvenID),
			(WORD)( m_pTINVEN->m_wItemID));

		m_pCommandHandler->m_vCOMMAND.push_back(
			TCOMMAND( GM_EXTEND_INVEN_PERIOD, dwPARAM));
	}

	if( msg != TNM_BASE )
	{
		TComponent *pFrom = FindKid(from);

		if( m_pCommandHandler &&
			pFrom && pFrom->m_menu[msg] != TCML_ID_NULL )
			m_pCommandHandler->m_vCOMMAND.push_back(pFrom->m_menu[msg]);
	}
}

BYTE CTInvenDlg::OnBeginDrag( LPTDRAG pDRAG,
							  CPoint point)
{
	CTCharInfoNewDlg* pChar = static_cast<CTCharInfoNewDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_CHARINFO_INNER));

	if( CTClientGame::m_bSendedCS_CASHITEMPUTIN_REQ )
		return FALSE;

	for( BYTE i=0; i<MAX_DRAGSLOT; i++)
	{
		TImageList *pHIT;
		if(m_bCharInfo)
			pHIT = dynamic_cast<TImageList*>(pChar->m_pFRAME->FindKid(CTClientGame::m_vSlotID[i]));
		else
			pHIT = dynamic_cast<TImageList*>(FindKid(CTClientGame::m_vSlotID[i]));

		CTClientItem* pTItem = m_pTINVEN->FindTItem( i );

		if( pHIT &&
			pHIT->HitTest(point) && ((TImageList *) pHIT)->GetCurImage() > 0 )
		{
			if( pDRAG )
			{
				CPoint pt;

				pDRAG->m_pIMAGE = new TImageList(
					NULL,
					*pHIT);

				pDRAG->m_pIMAGE->SetCurImage(((TImageList *) pHIT)->GetCurImage());
				pDRAG->m_pIMAGE->m_strText = ((TImageList *) pHIT)->m_strText;
				pDRAG->m_bSlotID = i;

				DWORD qt = 0;
				if ( pTItem )
					qt = pTItem->GetQuality();

				if( pTItem && pTItem->GetWrap() )
					pDRAG->m_pIMAGE->SetSkinImage( TITEM_INDEX_WRAP_SKIN );
				else if ( pTItem && (qt & TITEM_QUALITY_GRADE) )
					pDRAG->m_pIMAGE->SetSkinImage( TITEM_INDEX_GRADE_SKIN + pTItem->GetGrade() );
				else if ( pTItem && (qt & TITEM_QUALITY_RARE) )
					pDRAG->m_pIMAGE->SetSkinImage( TITEM_INDEX_RARE_SKIN );
				else if ( pTItem && (qt & TITEM_QUALITY_MAGIC) )
					pDRAG->m_pIMAGE->SetSkinImage( TITEM_INDEX_MAGIC_SKIN );
				else
					pDRAG->m_pIMAGE->SetSkinImageEmpty();

				TImageList* pICON = static_cast<TImageList*>( pHIT );
				pICON->GetComponentPos(&pt);
				pICON->ComponentToScreen(&pt);
				pICON->m_strText.Empty();
				pICON->SetCurImage(0);
				pICON->SetSkinImageEmpty();

				pDRAG->m_pIMAGE->ShowComponent(TRUE);
				pDRAG->m_pIMAGE->MoveComponent(pt);
			}

			return TRUE;
		}
	}

	return FALSE;
}

TDROPINFO CTInvenDlg::OnDrop( CPoint point)
{
	CTCharInfoNewDlg* pChar = static_cast<CTCharInfoNewDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_CHARINFO_INNER));
	TDROPINFO vResult;

	if(m_bDropTarget)
	{
		for( BYTE i=0; i<MAX_DRAGSLOT; i++)
		{
			TImageList *pDROP;
			if(m_bCharInfo)
				pDROP = dynamic_cast<TImageList*>(pChar->m_pFRAME->FindKid(CTClientGame::m_vSlotID[i]));
			else
				pDROP = dynamic_cast<TImageList*>(FindKid(CTClientGame::m_vSlotID[i]));

			if( pDROP && pDROP->HitTest(point) )
			{
				vResult.m_bSlotID = i;
				vResult.m_bDrop = TRUE;

				return vResult;
			}
		}

		// 못 찾았다면 이 인벤의 빈칸을 찾아서 그곳에 넣자.
		for( BYTE i=0; i<MAX_DRAGSLOT; i++)
		{
			TImageList* pDROP;
			if(m_bCharInfo)
				pDROP = dynamic_cast<TImageList*>(pChar->m_pFRAME->FindKid(CTClientGame::m_vSlotID[i]));
			else
				pDROP = dynamic_cast<TImageList*>(FindKid(CTClientGame::m_vSlotID[i]));

			if( pDROP && pDROP->GetCurImage() == 0 )
			{
				vResult.m_bSlotID = i;
				vResult.m_bDrop = TRUE;
				return vResult;
			}
		}
	}

	return vResult;
}

ITDetailInfoPtr CTInvenDlg::GetTInfoKey( const CPoint& point )
{
	ITDetailInfoPtr pInfo;
	CTCharInfoNewDlg* pChar = static_cast<CTCharInfoNewDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_CHARINFO_INNER));

	for( BYTE i=0; i<m_pTINVEN->m_pTITEM->m_bSlotCount; i++)
	{
		TImageList* pTICON = (TImageList *) FindKid(CTClientGame::m_vSlotID[i]);

		if( pTICON && pTICON->HitTest(point) )
		{
			CTClientItem* pTITEM = m_pTINVEN->FindTItem(i); 
			if( pTITEM )
			{
				CRect rc;
				CPoint pPoint;

				GetComponentRect(&rc);

				if(m_bCharInfo)
					pChar->GetComponentRect(&rc);

				pInfo = CTDetailInfoManager::NewItemInst(pTITEM, rc);
				if(!m_bCharInfo)
					pInfo->SetCanHandleSecondInfo( TRUE );


				pTICON->GetComponentPos(&pPoint);
				pTICON->ComponentToScreen(&pPoint);

				pInfo->SetDir(TRUE, TRUE, TRUE, m_bCharInfo ? TRUE : FALSE, m_bCharInfo ? pPoint : NULL);
			}

			break;
		}
	}

	return pInfo;
}

BOOL CTInvenDlg::GetTChatInfo( const CPoint& point, TCHATINFO& outInfo )
{
	CTCharInfoNewDlg* pChar = static_cast<CTCharInfoNewDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_CHARINFO_INNER));
	for( BYTE i=0; i<m_pTINVEN->m_pTITEM->m_bSlotCount; i++)
	{
		TImageList *pTICON;
		if(m_bCharInfo)
			pTICON = (TImageList *) pChar->m_pFRAME->FindKid(CTClientGame::m_vSlotID[i]);
		else
			pTICON = (TImageList *) FindKid(CTClientGame::m_vSlotID[i]);

		if(pTICON && pTICON->HitTest(point))
		{
			CTClientItem *pTITEM = m_pTINVEN->FindTItem(i);
			if(pTITEM)
			{
				outInfo.m_Type = TTEXT_LINK_TYPE_ITEM;
				outInfo.m_pItem = pTITEM->GetTITEM();
				outInfo.m_pClientItem = pTITEM;
				return TRUE;
			}

			return FALSE;
		}
	}

	return FALSE;
}

BOOL CTInvenDlg::CanWithItemUI()
{
	return TRUE;
}

void CTInvenDlg::MoveComponent( CPoint pt )
{
	CTClientUIBase::MoveComponent( pt );

	if (m_pSubInvenPos)
	{
		CRect rt;
		GetComponentRect(&rt);

		CTClientUIBase::GetUIOffset(
			CTClientUIBase::GetScreenRect(),
			rt,
			m_pSubInvenPos);
	}

	if (HIWORD(m_id) == TFRAME_DEFINVEN)
	{
		CRect rt;
		GetComponentRect(&rt);

		CTClientUIBase::GetUIOffset(
			CTClientUIBase::GetScreenRect(),
			rt,
			&m_pDefInven);
	}
}