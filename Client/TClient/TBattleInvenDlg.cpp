#include "StdAfx.h"
#include "Resource.h"
#include "TClientGame.h"
#include "TBattleInvenDlg.h"

CTBattleInvenDlg::CTBattleInvenDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, TCMLParser *pParser)
:CTInvenDlg( pParent, pDesc, 1206)
{
	m_pClose = (TButton*) FindKid(1276);
	//m_bCanDrag = TRUE;
}

CTBattleInvenDlg::~CTBattleInvenDlg()
{
}

ITDetailInfoPtr CTBattleInvenDlg::GetTInfoKey( const CPoint& point )
{
	ITDetailInfoPtr pInfo;

	for(BYTE i = 0; i < BATTLEINVEN_SLOT_COUNT * 2; i++)
	{
		TImageList* pTICON;
		if(i < BATTLEINVEN_SLOT_COUNT)
			pTICON = (TImageList*) FindKid(CTClientGame::m_vBattleSlotID[i]);
		else
			pTICON = (TImageList*) FindKid(CTClientGame::m_vSlotID[i - BATTLEINVEN_SLOT_COUNT]);

		if(pTICON && pTICON->HitTest(point) && m_pTINVEN)
		{
			CTClientItem* pTITEM = m_pTINVEN->FindTItem(i); 
			if(pTITEM)
			{
				CRect rc;
				GetComponentRect(&rc);

				pInfo = CTDetailInfoManager::NewItemInst(pTITEM, rc);
				pInfo->SetCanHandleSecondInfo(TRUE);
				pInfo->SetDir(TRUE, TRUE, TRUE);
			}

			break;
		}
	}

	return pInfo;
}

BOOL CTBattleInvenDlg::GetTChatInfo( const CPoint& point, TCHATINFO& outInfo )
{
	for(BYTE i = 0; i < BATTLEINVEN_SLOT_COUNT * 2; i++)
	{
		TImageList *pTICON;
		if(i < BATTLEINVEN_SLOT_COUNT)
			pTICON = (TImageList*) FindKid(CTClientGame::m_vBattleSlotID[i]);
		else
			pTICON = (TImageList*) FindKid(CTClientGame::m_vSlotID[i - BATTLEINVEN_SLOT_COUNT]);

		if(pTICON && pTICON->HitTest(point) && m_pTINVEN)
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

BYTE CTBattleInvenDlg::OnBeginDrag( LPTDRAG pDRAG,
							  CPoint point)
{
	for(BYTE i = 0; i < BATTLEINVEN_SLOT_COUNT * 2; i++)
	{
		TImageList *pHIT;
		if(i < BATTLEINVEN_SLOT_COUNT)
			pHIT = dynamic_cast<TImageList*>(FindKid(CTClientGame::m_vBattleSlotID[i]));
		else
			pHIT = dynamic_cast<TImageList*>(FindKid(CTClientGame::m_vSlotID[i - BATTLEINVEN_SLOT_COUNT]));

		CTClientItem* pTItem = m_pTINVEN->FindTItem(i);

		if(pHIT &&
			pHIT->HitTest(point) && ((TImageList *) pHIT)->GetCurImage() > 0)
		{
			if(pDRAG)
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

				TImageList* pICON = static_cast<TImageList*>(pHIT);
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

TDROPINFO CTBattleInvenDlg::OnDrop( CPoint point)
{
	TDROPINFO vResult;

	if(m_bDropTarget)
	{
		for( BYTE i = 0; i < BATTLEINVEN_SLOT_COUNT * 2; i++)
		{
			TImageList *pDROP;
			if(i < BATTLEINVEN_SLOT_COUNT)
				pDROP = dynamic_cast<TImageList*>(FindKid(CTClientGame::m_vBattleSlotID[i]));
			else
				pDROP = dynamic_cast<TImageList*>(FindKid(CTClientGame::m_vSlotID[i - BATTLEINVEN_SLOT_COUNT]));

			if(pDROP && pDROP->HitTest(point))
			{
				vResult.m_bSlotID = i;
				vResult.m_bDrop = TRUE;

				return vResult;
			}
		}
	}

	return vResult;
}

HRESULT CTBattleInvenDlg::Render( DWORD dwTickCount)
{
	if(IsVisible())
	{
		for( BYTE i = 0; i < BATTLEINVEN_SLOT_COUNT * 2; i++)
		{
			TGauge* pGAUGE;
			if(i < BATTLEINVEN_SLOT_COUNT)
				pGAUGE = (TGauge*) FindKid(CTClientGame::m_vBattleGauge[i]);
			else
				pGAUGE = (TGauge*) FindKid(CTClientGame::m_vTICONGAUGE[i - BATTLEINVEN_SLOT_COUNT]);

			if(pGAUGE)
			{
				CTClientItem *pTITEM = m_pTINVEN ? m_pTINVEN->FindTItem(i) : NULL;
				DWORD dwTick = pTITEM && pTITEM->GetTITEM() ? CTClientItem::GetTick(pTITEM->GetTITEM()->m_wDelayGroupID) : 0;

				if(dwTick)
				{
					pGAUGE->SetStyle(TGS_GROW_UP);
					pGAUGE->m_strText = CTClientGame::ToTimeString(dwTick); 
					pGAUGE->SetGauge(dwTick, pTITEM->GetTITEM()->m_dwDelay);

					pGAUGE->ShowComponent(TRUE);
				}
				else
					pGAUGE->ShowComponent(FALSE);
			}
		}
	}

	return CTClientUIBase::Render(dwTickCount);
}

void CTBattleInvenDlg::MoveComponent(CPoint pt)
{
	CTClientUIBase::MoveComponent(pt);
}

void CTBattleInvenDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	if (m_pClose &&
		m_pClose->HitTest(pt) &&
		m_pTINVEN)
	{
		CTClientGame::GetInstance()->DisableUI(TFRAME_BATTLEINVEN);
	}

	return CTClientUIBase::OnLButtonUp(nFlags, pt);
}