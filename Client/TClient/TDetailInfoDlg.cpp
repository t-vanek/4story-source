#include "StdAfx.h"
#include "TClientGame.h"
#include "Resource.h"


// Constants

constexpr int cGemXPosShiftOffset = 16;

// ====================================================================================================
CTDetailInfoDlg::CTDetailInfoDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, WORD wFrameID)
:	CTClientUIBase( pParent, pDesc), m_pAttrLevel(nullptr)
{

	if( m_id != 1120)
	{
		static const DWORD dwIconID[TDETAILINFO_ICON_COUNT] =
		{
			ID_CTRLINST_ITEM,
			ID_CTRLINST_SKILL,
			ID_CTRLINST_FACE,
			26269
		};

		for( BYTE i=0; i<TDETAILINFO_ICON_COUNT; i++)
			m_vICON[i] = dynamic_cast<TImageList*>( FindKid(dwIconID[i]) );

		m_arGem[eDetailInfoGem_1] = dynamic_cast<TImageList*>( FindKid(ID_CTRLINST_GEM_1) );
		m_arGem[eDetailInfoGem_1]->ShowComponent(FALSE);

		CPoint ptGem0;
		m_arGem[eDetailInfoGem_1]->GetComponentPos(&ptGem0);

		for (BYTE i = eDetailInfoGem_1; i < eDetailInfoGem_count; ++i)
		{
			m_arGem[i] = new TImageList(this, m_arGem[eDetailInfoGem_1]->m_pDESC);
			m_arGem[i]->m_id = GetUniqueID();
			m_arGem[i]->MoveComponent(CPoint(ptGem0.x + i * cGemXPosShiftOffset, ptGem0.y));

			AddKid(m_arGem[i]);
			m_arGem[i]->ShowComponent(FALSE);
		}

		CPoint pt;

		m_vICON[0]->GetComponentPos(&pt);
		m_nTitlePosX_NIC = pt.x;

		m_pTEXT = FindKid(ID_CTRLINST_TEXT);
		RemoveKid(m_pTEXT);

		m_pNAME = FindKid(ID_CTRLINST_NAME);
		m_pNAME->GetComponentPos(&pt);
		m_nTitlePosY = pt.y;
		m_nTitlePosX_NOR = pt.x;

		m_pSTART = FindKid(ID_CTRLINST_START);
		m_pEND = FindKid(ID_CTRLINST_END);
		m_pBACK = FindKid(ID_CTRLINST_BACK);

		m_pCLOSE = FindKid(ID_CTRLINST_INFO_CLOSE);
		ShowCloseButton(FALSE);
	}
	else
	{
		static const DWORD dwIconID[TDETAILINFO_ICON_COUNT] =
		{
			26213,
			26214, //2182
			26212,
			26271
		};

		for( BYTE i=0; i<TDETAILINFO_ICON_COUNT; i++)
			m_vICON[i] = dynamic_cast<TImageList*>( FindKid(dwIconID[i]) );

		m_arGem[eDetailInfoGem_1] = dynamic_cast<TImageList*>(FindKid(26216));
		m_arGem[eDetailInfoGem_1]->ShowComponent(FALSE);

		CPoint ptGem0;
		m_arGem[eDetailInfoGem_1]->GetComponentPos(&ptGem0);

		for (BYTE i = eDetailInfoGem_1; i < eDetailInfoGem_count; ++i)
		{
			m_arGem[i] = new TImageList(this, m_arGem[eDetailInfoGem_1]->m_pDESC);
			m_arGem[i]->m_id = GetUniqueID();
			m_arGem[i]->MoveComponent(CPoint(ptGem0.x + i * cGemXPosShiftOffset, ptGem0.y));

			AddKid(m_arGem[i]);
			m_arGem[i]->ShowComponent(FALSE);
		}


		CPoint pt;

		m_vICON[0]->GetComponentPos(&pt);
		m_nTitlePosX_NIC = pt.x;

		m_pTEXT = FindKid(26209);
		RemoveKid(m_pTEXT);

		m_pNAME = FindKid(26215); //863
		m_pNAME->GetComponentPos(&pt);

		m_nTitlePosY = pt.y;
		m_nTitlePosX_NOR = pt.x;

		m_pSTART = FindKid(26207); //744
		m_pEND = FindKid(26210);
		m_pBACK = FindKid(26208);

		m_pCLOSE = FindKid(26211);
		ShowCloseButton(FALSE);
	}

	if (m_pNAME)
	{
		CPoint pt;
		m_vICON[TDETAILINFO_ICON_ITEM]->GetComponentPos(&pt);
		pt.x += m_vICON[TDETAILINFO_ICON_ITEM]->m_rc.Height() - 19;
		pt.y += m_vICON[TDETAILINFO_ICON_ITEM]->m_rc.Width() - 25;

		//FRAMEDESC_SHAREDPTR pDESC = new FRAMEDESC(*m_pNAME->m_pDESC);
		//pDESC->m_vCOMP.m_nWidth = 16;
		m_pAttrLevel = new TComponent(this, m_pNAME->m_pDESC); // ToDo
		m_pAttrLevel->m_id = GetUniqueID();
		m_pAttrLevel->SetTextClr(0xFF80CCFD);
		m_pAttrLevel->MoveComponent(pt);
		m_pAttrLevel->SetTextAlign(ALIGN_RIGHT);

		CRect rc;
		m_pAttrLevel->GetComponentRect(&rc);
		rc.right = rc.left + 16;
		m_pAttrLevel->SetComponentRect(rc);
		AddKid(m_pAttrLevel);
	}

	CRect rt;
	m_pSTART->GetComponentRect( &rt );
	m_ptUNTITLESTART.x = rt.left;
	m_ptUNTITLESTART.y = rt.bottom;
	m_pTEXT->GetComponentRect( &rt );
	m_ptTITLESTART.x = rt.left;
	m_ptTITLESTART.y = rt.top;
	m_ptTextStart = m_ptTITLESTART;
	m_pTEXT->GetComponentSize( &m_vTEXTSIZE );

	m_vTEXT.clear();
	ClearText();

    m_pNAMEFONT = m_pNAME->Get3DFont();
	m_bSND = FALSE;
}
// ----------------------------------------------------------------------------------------------------
CTDetailInfoDlg::~CTDetailInfoDlg()
{
	ClearText();
}
// ====================================================================================================
void CTDetailInfoDlg::ResetINFO(ITDetailInfoPtr pInfo)
{
	ClearText();

	pInfo->Build();
	INT nOutlookStyle = pInfo->GetOutlookStyle();

	switch( nOutlookStyle )
	{
	case TDETINFO_OUTLOOK_STYLE_TITLE:
		{
			m_pBACK->ShowComponent(TRUE);
			m_ptTextStart = m_ptTITLESTART;
			m_bNoHIT = FALSE;
		}
		break;
	case TDETINFO_OUTLOOK_STYLE_UNTITLE:
		{
			m_pBACK->ShowComponent(FALSE);
			m_ptTextStart = m_ptUNTITLESTART;
			m_bNoHIT = TRUE;
		}
		break;
	}

	CSize szTEXT;
	CString strFORMAT;

	CSize szITEM( m_vTEXTSIZE );
	szITEM.cx -= 2 * TINFOTEXT_MARGINE;

	UINT nLineCnt = pInfo->GetLineCount();
	for(UINT i=0; i<nLineCnt; ++i)
	{
		CString strLINE = pInfo->GetLineText(i);
		DWORD dwCOLOR = pInfo->GetLineColor(i);

		VECTORSTRING vLines;

		RECT rtRC;
		m_pTEXT->GetComponentRect( &rtRC );
		m_pTEXT->SeparateTextFitSize( strLINE, rtRC.right-rtRC.left-10, vLines);

		for( INT i=0 ; i < vLines.size() ; ++i )
			AddText( vLines[i], dwCOLOR );
	}

	BYTE bIconID = GetIconID(pInfo);
	WORD wImageID = pInfo->GetImageID();


	CPoint vPos( m_ptTextStart );
	vPos.y += m_vTEXT.size() * m_vTEXTSIZE.cy;
	m_pEND->MoveComponent( vPos );

	BOOL bShowIcon = FALSE;
	for( INT i=0; i<TDETAILINFO_ICON_COUNT; ++i )
	{
		if( nOutlookStyle == TDETINFO_OUTLOOK_STYLE_UNTITLE )
		{
			m_vICON[i]->ShowComponent(wImageID);
			continue;
		}

		if( bIconID == i )
		{
			INT nSkinIndex = pInfo->CheckUsingSkin();
			if( T_INVALID == nSkinIndex  )
				m_vICON[i]->SetSkinImageEmpty();
			else
				m_vICON[i]->SetSkinImage( nSkinIndex );

			m_vICON[i]->SetCurImage(wImageID);
			m_vICON[i]->ShowComponent(TRUE);
			bShowIcon = TRUE;
		}
		else
			m_vICON[i]->ShowComponent(FALSE);
	}

	int nGemRange = -1;

	switch (pInfo->GetType())
	{
	case TDEFINFO_TYPE_GEM_0:
	case TDEFINFO_TYPE_GEM_0_W:
		nGemRange = 0; break;

	case TDEFINFO_TYPE_GEM_1:
	case TDEFINFO_TYPE_GEM_1_W:
		nGemRange = 1; break;

	case TDEFINFO_TYPE_GEM_2:
	case TDEFINFO_TYPE_GEM_2_W:
		nGemRange = 2; break;

	case TDEFINFO_TYPE_GEM_3:
	case TDEFINFO_TYPE_GEM_3_W:
		nGemRange = 3; break;

	case TDEFINFO_TYPE_GEM_4:
	case TDEFINFO_TYPE_GEM_4_W:
		nGemRange = 4; break;

	case TDEFINFO_TYPE_GEM_5:
	case TDEFINFO_TYPE_GEM_5_W:
		nGemRange = 5; break;

	default:
		break;
	}

	for (size_t i = 0; i < eDetailInfoGem_count; ++i)
	{
		if (nGemRange != -1)
		{
			if (i >= nGemRange)
				m_arGem[i]->SetCurImage(eGemImage_none);
			else
				m_arGem[i]->SetCurImage(eGemImage_red);

			RemoveKid(m_arGem[i]);
			AddKid(m_arGem[i]);
			m_arGem[i]->ShowComponent(TRUE);
		}
		else
			m_arGem[i]->ShowComponent(FALSE);
	}

	if( bShowIcon )
	{
		m_pNAME->MoveComponent( CPoint(m_nTitlePosX_NOR, m_nTitlePosY) );
		CTClientItem* pTITEM = NULL;
		if (dynamic_cast<CTItemInstDetInfo*> (pInfo.get()) != NULL)
			pTITEM = static_cast<CTItemInstDetInfo*>(pInfo.get())->GetItemInst();

		if (pTITEM && pTITEM->GetTITEM() && pTITEM->GetAttrID())
		{
			RemoveKid(m_pAttrLevel);
			AddKid(m_pAttrLevel);

			BYTE bBaseAttr = CTChart::FindTITEMATTR(pTITEM->GetAttrID() - CTChart::m_vTITEMGRADE[pTITEM->GetGrade()].m_bGrade - pTITEM->GetGem())->m_bGrade;
			if (bBaseAttr == 0)
				m_pAttrLevel->ShowComponent(FALSE);
			else
				m_pAttrLevel->ShowComponent(TRUE);

			m_pAttrLevel->m_strText.Format("%d", bBaseAttr);
		}
		else
			m_pAttrLevel->ShowComponent(FALSE);
	}
	else
	{
		m_pAttrLevel->ShowComponent(FALSE);
		m_pNAME->MoveComponent( CPoint(m_nTitlePosX_NIC, m_nTitlePosY) );
	}

	m_pNAME->m_strText = pInfo->GetTitleText();
	m_pNAMEFONT->m_dwColor = pInfo->GetTitleColor();

	TComponent* pWearing = FindKid(26217);
	if(pWearing)
		AddKid(pWearing);
}
// ----------------------------------------------------------------------------------------------------
void CTDetailInfoDlg::ShowCloseButton(BOOL bShow)
{
	m_pCLOSE->ShowComponent(bShow);
}
// ====================================================================================================
BOOL CTDetailInfoDlg::CanWithItemUI()
{
	return TRUE;
}
// ----------------------------------------------------------------------------------------------------
void CTDetailInfoDlg::ShowComponent( BOOL bVisible)
{
	m_bVisible = bVisible;
}
// ----------------------------------------------------------------------------------------------------
void CTDetailInfoDlg::GetComponentRect( LPRECT lpRect)
{
	*lpRect = m_rc;

	if(m_pEND)
	{
		CRect rect;

		m_pEND->GetComponentRect(&rect);
		lpRect->bottom = lpRect->top + rect.bottom;
	}
}
// ====================================================================================================
void CTDetailInfoDlg::AddText(const CString& strTEXT, DWORD dwColor)
{
	TComponent* pTEXT = new TComponent(this, *m_pTEXT);
	int nCount = INT(m_vTEXT.size());

	pTEXT->m_id = MAKELONG(WORD(m_id), WORD(nCount+1));
	pTEXT->m_strText = strTEXT;

	CD3DFont* pFont = pTEXT->Get3DFont();
	if( pFont )
		pFont->m_dwColor = dwColor;

	CRect rect;
	rect.left = m_ptTextStart.x;
	rect.top = m_ptTextStart.y + (nCount*m_vTEXTSIZE.cy);
	rect.right = rect.left + m_vTEXTSIZE.cx;
	rect.bottom = rect.top + m_vTEXTSIZE.cy;
	pTEXT->SetComponentRect(&rect);
	pTEXT->m_bWordBreak = FALSE;

	m_vTEXT.push_back(pTEXT);
	AddKid(pTEXT);
}
// ----------------------------------------------------------------------------------------------------
void CTDetailInfoDlg::ClearText()
{
	m_pNAME->m_strText.Empty();

	while(!m_vTEXT.empty())
	{
		RemoveKid(m_vTEXT.back());
		delete m_vTEXT.back();

		m_vTEXT.pop_back();
	}

	m_blDragging = FALSE;
	m_pFocus = NULL;

	m_ptPrev = CPoint( 0, 0);
	m_tip.Reset();
}
// ----------------------------------------------------------------------------------------------------
BYTE CTDetailInfoDlg::GetIconID( ITDetailInfoPtr pInfo)
{

	switch( pInfo->GetType() )
	{
	case TDETINFO_TYPE_ITEM:
	case TDEFINFO_TYPE_INSTITEM:
	case TDEFINFO_TYPE_GEM_0:
	case TDEFINFO_TYPE_GEM_1:
	case TDEFINFO_TYPE_GEM_2:
	case TDEFINFO_TYPE_GEM_3:
	case TDEFINFO_TYPE_GEM_4:
	case TDEFINFO_TYPE_GEM_5:
	case TDEFINFO_TYPE_GEM_0_W:
	case TDEFINFO_TYPE_GEM_1_W:
	case TDEFINFO_TYPE_GEM_2_W:
	case TDEFINFO_TYPE_GEM_3_W:
	case TDEFINFO_TYPE_GEM_4_W:
	case TDEFINFO_TYPE_GEM_5_W:
	case TDETINFO_TYPE_SEALEDITEM:
	case TDETINFO_TYPE_OPTIONITEM:
		return TDETAILINFO_ICON_ITEM;

	case TDETINFO_TYPE_SKILL:
		return TDETAILINFO_ICON_SKILL;

	case TDETINFO_TYPE_PLAYER:
		return TDETAILINFO_ICON_FACE;

	case TDEFINFO_TYPE_COMPANION:
		return TDETAILINFO_ICON_MONSTER;
	}

	return TDETAILINFO_ICON_NONE;
}
// ====================================================================================================
ITDetailInfoPtr CTDetailInfoDlg::GetTInfoKey( const CPoint& point )
{
	return ITDetailInfoPtr(); //return CTDetailInfoManager::m_pLastInfo;
}

void CTDetailInfoDlg::OnLButtonDown(UINT nFlags, CPoint pt)
{
	CTDetailInfoManager::m_dwInfoStaticTick = 0;
	CTClientUIBase::OnLButtonDown(nFlags, pt);
}