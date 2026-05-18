// TImageList.cpp: implementation of the TImageList class.
//
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"


/////////////////////////////////////////// Static ///////////////////////////////////////////
TImageList::TDELEGATEMAP TImageList::s_mapDELEGATE;

TImageList::DELEGATE_SHAREDPTR TImageList::CreateDelegate(FRAMEDESC_SHAREDPTR rpDesc)
{
	if (!rpDesc)
		return nullptr;

	auto itDELEGATE = s_mapDELEGATE.find(rpDesc);

	if (itDELEGATE != s_mapDELEGATE.end())
		return (*itDELEGATE).second;

	TImageList::DELEGATE_SHAREDPTR pDelegate(new TDelegate(rpDesc));
	s_mapDELEGATE.insert(std::make_pair(rpDesc, pDelegate));

	return pDelegate;
}

void TImageList::ReleaseDelegate(const TImageList::DELEGATE_SHAREDPTR& rpDelegate)
{
	if (!rpDelegate)
		return;

	for (auto itDELEGATE : s_mapDELEGATE)
	{
		if (itDELEGATE.second == rpDelegate)
		{
			if (rpDelegate.use_count() == 2)
			{
				s_mapDELEGATE.erase(itDELEGATE.first);
				return;
			}
		}
	}
}

void TImageList::ClearDelegates()
{
	s_mapDELEGATE.clear();
}

//////////////////////////////////////////////////////////////////////////////////////////////

// ====================================================================================================
bool TImageList::tFrameCmp::operator () (FRAMEDESC_SHAREDPTR lhs, FRAMEDESC_SHAREDPTR rhs) const
{
	if (lhs->m_vCOMP.m_bType != rhs->m_vCOMP.m_bType)
		return lhs->m_vCOMP.m_bType < rhs->m_vCOMP.m_bType;

	if (lhs->m_vCOMP.m_nWidth != rhs->m_vCOMP.m_nWidth)
		return lhs->m_vCOMP.m_nWidth < rhs->m_vCOMP.m_nWidth;

	if (lhs->m_vCOMP.m_nHeight != rhs->m_vCOMP.m_nHeight)
		return lhs->m_vCOMP.m_nHeight < rhs->m_vCOMP.m_nHeight;

	FRAMEDESC_SHAREDPTR pLkid = lhs->m_pCHILD;
	FRAMEDESC_SHAREDPTR pRkid = rhs->m_pCHILD;

	while (pLkid && pRkid)
	{
		if (pLkid->m_vCOMP.m_dwID != pRkid->m_vCOMP.m_dwID)
			return pLkid->m_vCOMP.m_dwID < pRkid->m_vCOMP.m_dwID;

		pLkid = pLkid->m_pNEXT;
		pRkid = pRkid->m_pNEXT;
	}

	return (pRkid != nullptr);
}

// TImageList::TDelegate
// ====================================================================================================
TImageList::TDelegate::TDelegate(FRAMEDESC_SHAREDPTR pDesc)
	: TComponent(NULL, pDesc)
{

}
// ----------------------------------------------------------------------------------------------------
TImageList::TDelegate::~TDelegate()
{

}
// ====================================================================================================
TComponent* TImageList::TDelegate::RenderImgList( TImageList* pImgList, DWORD dwCurTick, DWORD dwDeltaTick)
{
	int nIndex = pImgList->GetCurImage();

	if (nIndex == -1)
		return nullptr;

	if (!m_kids.size())
		return nullptr;

	TComponent *pImgComp = nullptr;

	if (nIndex >= 60000) // This is really shitty. Rather mark it as own image instead of using this upper limit.
	{
		pImgComp = FindKid(nIndex);

		if (!pImgComp)
		{
			pImgComp = new TComponent(this, *m_kids.front());
			pImgComp->m_id = nIndex;
			pImgComp->UseOwnImages(nIndex);
			m_kids.push_back(pImgComp);
		}
	}
	else
	{
		TCOMP_LIST::iterator itr = GetFirstKidsFinder();

		if (EndOfKids(itr))
			return nullptr;

		if (nIndex >= GetKidsCount())
			return nullptr;

		std::advance(itr, nIndex);

		if (EndOfKids(itr))
			return nullptr;

		pImgComp = *itr;
	}

	if (!pImgComp)
		return nullptr;

	RenderImgComp(pImgList, pImgComp, dwCurTick, dwDeltaTick);
	return pImgComp;
}
// ----------------------------------------------------------------------------------------------------
void TImageList::TDelegate::RenderImgComp( TImageList* pImgList, TComponent* pImgComp, DWORD dwCurTick, DWORD dwDeltaTick)
{
	m_pParent = pImgList->GetParent();

	POINT pt;
	pImgList->GetComponentPos(&pt);
	MoveComponent(pt);

	m_dwTotalTick = dwCurTick;

	if( pImgList->IsUserColorEnabled() )
	{
		pImgComp->SetStyle( pImgComp->GetStyle() | TS_CUSTOM_COLOR );
		pImgComp->m_dwColor = pImgList->GetUserColor();
	}
	else
		pImgComp->SetStyle( pImgComp->GetStyle() & ~TS_CUSTOM_COLOR );
	
	pImgComp->EnableComponent( pImgList->IsEnable() );
	pImgComp->Render(dwDeltaTick);
}
// ====================================================================================================
BOOL TImageList::TDelegate::HitRectDelegate( TImageList* pImgList, CPoint pt)
{
	m_pParent = pImgList->GetParent();

	POINT ptImgList;
	pImgList->GetComponentPos(&ptImgList);
	MoveComponent(ptImgList);

	return TComponent::HitRect(pt);
}
// ----------------------------------------------------------------------------------------------------
BOOL TImageList::TDelegate::HitTestDelegate( TImageList* pImgList, CPoint pt)
{
	m_pParent = pImgList->GetParent();

	POINT ptImgList;
	pImgList->GetComponentPos(&ptImgList);
	MoveComponent(ptImgList);

	return TComponent::HitTest(pt);
}
// ====================================================================================================
int TImageList::TDelegate::GetImageCount() const
{
	return 99999999;
}
// ====================================================================================================
TComponent* TImageList::TDelegate::GetImage( int nIndex ) const
{
	auto itr = GetFirstKidsFinder();
	std::advance(itr, nIndex);

	if( !EndOfKids(itr) )
		return *itr;

	return NULL;
}

// ====================================================================================================
TImageList::TImageList(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc)
:	TComponent(pParent, pDesc, false), 
	m_nCurIdx(0),
	m_nLastIdx(0),
	m_pLastImg(NULL),
	m_pSkinImg(NULL),
	m_bUserColor(FALSE),
	m_dwUserColor(0),
	m_bUsePixelHitTest(FALSE)	
{
	m_bType = TCML_TYPE_IMAGELIST;
	m_pDelegate = CreateDelegate(pDesc);
	InitRect();
}

TImageList::TImageList(TComponent* pParent, FRAMEDESC_WEAKPTR pDesc)
	: TImageList(pParent, pDesc.lock())
{

}

// ----------------------------------------------------------------------------------------------------
TImageList::TImageList(TComponent* pParent, const TImageList& rSrcImgLst)
	: TComponent(pParent, rSrcImgLst),
	m_nCurIdx(0),
	m_nLastIdx(0),
	m_pLastImg(NULL),
	m_pSkinImg(NULL),
	m_bUserColor(FALSE),
	m_dwUserColor(0),
	m_bUsePixelHitTest(FALSE)
{
	m_pDelegate = rSrcImgLst.m_pDelegate;
	InitRect();
}
// ----------------------------------------------------------------------------------------------------
TImageList::~TImageList()
{
	ReleaseDelegate(m_pDelegate);
}
// ====================================================================================================
BOOL TImageList::HitRect( CPoint pt)
{
	return m_pDelegate->HitRectDelegate(this,pt);
}
// ----------------------------------------------------------------------------------------------------
BOOL TImageList::HitTest( CPoint pt)
{
	if( IsVisible() )
		return m_bUsePixelHitTest ? m_pDelegate->HitTestDelegate(this,pt) : m_pDelegate->HitRectDelegate(this,pt);
	else
		return FALSE;
}
// ----------------------------------------------------------------------------------------------------
HRESULT TImageList::Render( DWORD dwTickCount)
{
	if( !IsVisible() )
		return S_OK;

	m_dwTotalTick += dwTickCount;

	if( m_pLastImg && m_nCurIdx == m_nLastIdx )
		m_pDelegate->RenderImgComp(this, m_pLastImg, m_dwTotalTick, dwTickCount);
	else
	{
		m_pLastImg = m_pDelegate->RenderImgList(this, m_dwTotalTick, dwTickCount);
		m_nLastIdx = m_nCurIdx;
	}

	if( m_pSkinImg )
		m_pDelegate->RenderImgComp( this, m_pSkinImg, m_dwTotalTick, dwTickCount );

	return DrawText();
}
// ====================================================================================================
BOOL TImageList::EndOfImgs( TCOMP_LIST::iterator it )
{
	return m_pDelegate->EndOfKids(it);
}
// ----------------------------------------------------------------------------------------------------
TCOMP_LIST::iterator TImageList::GetFirstImgsFinder()
{
	return m_pDelegate->GetFirstKidsFinder();
}
// ----------------------------------------------------------------------------------------------------
TComponent* TImageList::GetNextImg( TCOMP_LIST::iterator &it )
{
	return m_pDelegate->GetNextKid(it);
}
// ====================================================================================================
void TImageList::InitRect()
{
	if (!m_pDelegate)
		return;

	CRect rcDelegate;
	m_pDelegate->GetComponentRect(&rcDelegate);

	m_rc.right  = m_rc.left + rcDelegate.Width();
	m_rc.bottom = m_rc.top  + rcDelegate.Height();

	m_nTextExtent = m_rc.Width() - 2 * m_nHMargine;
}
// ====================================================================================================
void TImageList::SetSkinImage( int nIndex )
{
	m_pSkinImg = m_pDelegate->GetImage( nIndex );	
}
// ====================================================================================================
void TImageList::SetSkinImageEmpty()
{
	m_pSkinImg = NULL;
}