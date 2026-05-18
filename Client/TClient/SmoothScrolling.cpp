#include "stdafx.h"
#include "SmoothScrolling.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "TClientUIBase.h"

SmoothScrolling::SmoothScrolling(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc)
	: TScroll(pParent, pDesc)
{
	m_vComponentList.clear();
	m_vImageList.clear();
}

SmoothScrolling::~SmoothScrolling()
{
	for (auto Component : m_vComponentList)
		delete Component.m_pComponent;

	for (auto Component : m_vImageList)
		delete Component.m_pImage;

	m_vComponentList.clear();
	m_vImageList.clear();
}

void SmoothScrolling::Setup(CRect rArea, DWORD* All, BYTE bDynObjects, CTClientUIBase* pSource)
{
	m_pSource = pSource;
	const BYTE Size = 2;
	BYTE bHeight = pSource->FindKid(All[0])->GetDefaultImage()->GetImage()->GetHeight();

	for (BYTE i = 0; i < Size; ++i)
	{
		for (BYTE o = 1; o < bDynObjects; ++o)
		{
			TComponent* pComponentBase = pSource->FindKid(All[i]);
			if (!pComponentBase) {
				AfxMessageBox(":D");
				continue;
			}

			switch (pSource->FindKid(All[i])->m_bType)
			{
			case TCML_TYPE_COMPONENT:
			{
				TComponent* pItem = new TComponent(pSource, pComponentBase->m_pDESC);
				pItem->m_id = pSource->GetUniqueID();
				pItem->MoveComponentBy(0, o * bHeight);

				pSource->AddKid(pItem);

				SmoothData Data;
				Data.m_bID = i;
				Data.m_bIndex = o;
				Data.m_pComponent = pItem;

				m_vComponentList.push_back(Data);
			}
			break;
			case TCML_TYPE_IMAGELIST:
			{
				TImageList* pItem = new TImageList(pSource, pComponentBase->m_pDESC);
				pItem->m_id = pSource->GetUniqueID();
				pItem->MoveComponentBy(0, o * bHeight);

				pSource->AddKid(pItem);
				static_cast<TImageList*>(pItem)->SetCurImage(rand() % 1000);

				SmoothData Data;
				Data.m_bID = i;
				Data.m_bIndex = o;
				Data.m_pImage = pItem;

				m_vImageList.push_back(Data);
			}
			break;
			}

		}
	}

	pSource->SmoothCallback(m_vComponentList, m_vImageList);
}

BOOL SmoothScrolling::DoMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{

	return TRUE;
}

void SmoothScrolling::OnNotify(DWORD from, WORD msg, LPVOID param)
{

}

void SmoothScrolling::AddItem(BYTE bID, BYTE bIndex, WORD wImage)
{

}

void SmoothScrolling::AddItem(BYTE bID, BYTE bIndex, CString strText)
{

}
