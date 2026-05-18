#include "StdAfx.h"
#include "TItemCraftDlg.h"
#include "TClientGame.h"

#define ID_CTRLINST_IC_REWARDTITLE			(0x00006BC2)
#define ID_CTRLINST_IC_REWARDICONS			(0x00006BC4)
#define ID_CTRLINST_IC_REWARDITEMNAME		(0x00006BC5)

#define ID_CTRLINST_IC_CREATEBTN			(0x00006BB2)
#define ID_CTRLINST_IC_DLGTITLE				(0x00000013)
#define ID_CTRLINST_IC_LISTTITLE			(0x00006BB5)
#define ID_CTRLINST_IC_NEEDEDTITLE			(0x000065EA)
#define ID_CTRLINST_IC_NEEDEDDESTITLE		(0x00006BBF)
#define ID_CTRLINST_IC_REWARDLISTITEMNAME	(0x00006BBA)
#define ID_CTRLINST_IC_MATERIALLISTTITLE	(0x00006BBD)
#define ID_CTRLINST_IC_MATERIALLISTCOUNT	(0x00006BBE)
#define ID_CTRLINST_IC_REWARDLISTARROW		(0x00006BB8)
#define ID_CTRLINST_IC_REWARDLISTBTN		(0x00006BB9)

#define ID_CTRLINST_IC_CATBTN				(0x00006D17) 
#define ID_CTRLINST_IC_CATBTNPRESSED		(0x00006D18) //GEDRÜCKTER(Deppresiver) CATBTN

CTItemCraftDlg::CTItemCraftDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
	: CTClientUIBase(pParent, pDesc)
{
	m_pRewardTitle = static_cast<TComponent *>(FindKid(ID_CTRLINST_IC_REWARDTITLE));
	m_pRewardIcon = static_cast<TImageList *>(FindKid(ID_CTRLINST_IC_REWARDICONS));
	m_pRewardItemTitle = static_cast<TComponent *>(FindKid(ID_CTRLINST_IC_REWARDITEMNAME));

	m_pCreateBtn = static_cast<TButton *>(FindKid(ID_CTRLINST_IC_CREATEBTN));
	m_pDlgTitle = static_cast<TComponent *>(FindKid(ID_CTRLINST_IC_DLGTITLE));
	m_pListTitle = static_cast<TComponent *>(FindKid(ID_CTRLINST_IC_LISTTITLE));
	m_pNeededTitle = static_cast<TComponent *>(FindKid(ID_CTRLINST_IC_NEEDEDTITLE));
	m_pNeededDesTitle = static_cast<TComponent *>(FindKid(ID_CTRLINST_IC_NEEDEDDESTITLE));
	m_pMateriallistTitle = static_cast<TList *>(FindKid(ID_CTRLINST_IC_MATERIALLISTTITLE));
	m_pMateriallistCount = static_cast<TList *>(FindKid(ID_CTRLINST_IC_MATERIALLISTCOUNT));	

	m_pRewardTitle->m_strText = "Result";
	m_pRewardIcon->SetCurImage(1337);
	m_pRewardItemTitle->m_strText = "Essence of Health(1200)";
	m_pCreateBtn->m_strText = "Create";
	m_pDlgTitle->m_strText = "Craft Item";
	m_pListTitle->m_strText = "List";
	m_pNeededTitle->m_strText = "Needed Item";
	m_pNeededDesTitle->m_strText = "How to Acquire";

	m_pMateriallistTitle->m_strText = "Test Materialitem 1";
	m_pMateriallistCount->m_strText = "69/88";

	m_pRewardItem[0] = static_cast<TComponent *>(FindKid(ID_CTRLINST_IC_REWARDLISTITEMNAME));
	m_pRewardArrow[0] = static_cast<TComponent *>(FindKid(ID_CTRLINST_IC_REWARDLISTARROW));
	m_pRewardBtn[0] = static_cast<TButton *>(FindKid(ID_CTRLINST_IC_REWARDLISTBTN));
	m_pCatBtn[0] = static_cast<TButton *>(FindKid(ID_CTRLINST_IC_CATBTN));
	m_pCatBtnPressed[0] = static_cast<TButton *>(FindKid(ID_CTRLINST_IC_CATBTNPRESSED));

	m_bCategoryCount = 0;
	m_bRewardCount = 0;
	m_bMaterialCount = 0;
}

CTItemCraftDlg::~CTItemCraftDlg()
{
	for(BYTE i=0; i < m_vCraftReward.size(); i++)
		m_vCraftReward[i].m_vMaterial.clear();

	m_vCraftReward.clear();
}

void CTItemCraftDlg::AddRewardItem(WORD wID, BYTE bCat, DWORD dwItemID )
{
	CPoint ptRewardBtn, ptRewardArrow, ptRewardItem;
	LPTITEM m_pItem = new TITEM();
	m_pItem = CTChart::FindTITEMTEMP( (WORD)dwItemID );

	m_pRewardBtn[0]->GetComponentPos( &ptRewardBtn );
	m_pRewardArrow[0]->GetComponentPos( &ptRewardArrow );
	m_pRewardItem[0]->GetComponentPos( &ptRewardItem );

	m_bRewardCount += 1;

	CRAFTREWARD pReward;
	pReward.m_bCategory = bCat;
	pReward.m_bRewardBtn = m_bRewardCount;
	pReward.m_dwItemID = dwItemID;
	pReward.m_wID = wID;

	m_vCraftReward.push_back( pReward );

	m_pRewardBtn[m_bRewardCount] = new TButton( this, *m_pRewardBtn[0]);
	m_pRewardArrow[m_bRewardCount] = new TComponent( this, *m_pRewardArrow[0]);
	m_pRewardItem[m_bRewardCount] = new TComponent( this, *m_pRewardItem[0]);

	m_pRewardBtn[m_bRewardCount]->m_id = GetUniqueID();
	m_pRewardArrow[m_bRewardCount]->m_id = GetUniqueID();
	m_pRewardItem[m_bRewardCount]->m_id = GetUniqueID();
	
	AddKid( m_pRewardBtn[m_bRewardCount] );
	AddKid( m_pRewardArrow[m_bRewardCount] );
	AddKid( m_pRewardItem[m_bRewardCount] );

	//TEST SHIT
	m_pRewardItem[m_bRewardCount]->m_strText = m_pItem->m_strNAME;
	//TEST SHIT END

	if(m_bRewardCount > 0)
	{
		ptRewardItem.y +=  (( m_bRewardCount - 1 ) * 20) + (( bCat - 1 ) * 30);
		ptRewardArrow.y += (( m_bRewardCount - 1 ) * 20) + (( bCat - 1 ) * 30);
		ptRewardBtn.y += (( m_bRewardCount - 1 ) * 20) + (( bCat - 1 ) * 30);
		m_pRewardBtn[m_bRewardCount]->MoveComponent( ptRewardBtn );
		m_pRewardArrow[m_bRewardCount]->MoveComponent( ptRewardArrow );
		m_pRewardItem[m_bRewardCount]->MoveComponent( ptRewardItem );
	}
	
}

void CTItemCraftDlg::AddMaterial( DWORD dwItemID, WORD wCount )
{
	
}

void CTItemCraftDlg::AddCategory( CString strName )
{
	CPoint ptCatBtn;

	m_pCatBtn[0]->GetComponentPos( &ptCatBtn );

	m_bCategoryCount += 1;

	m_pCatBtn[m_bCategoryCount] = new TButton( this, *m_pCatBtn[0]);
	m_pCatBtn[m_bCategoryCount]->m_id = GetUniqueID();

	AddKid( m_pCatBtn[m_bCategoryCount] );
	
	m_pCatBtn[m_bCategoryCount]->m_strText = strName;

	if(m_bCategoryCount > 1)
	{
		ptCatBtn.y += ( m_bCategoryCount - 1 ) * 30;
		m_pCatBtn[m_bCategoryCount]->MoveComponent( ptCatBtn );
	}	
}

DWORD CTItemCraftDlg::CharItemCount( DWORD dwItemID )
{
	CTClientChar* pChar = CTClientGame::GetInstance()->GetMainChar();

	MAPTINVEN::iterator it = pChar->m_mapTINVEN.begin();
	MAPTINVEN::iterator end = pChar->m_mapTINVEN.end();

	DWORD dwCount = 0;

	while( it != end )
	{
		MAPTITEM::iterator item_it = (*it).second->m_mapTITEM.begin();
		MAPTITEM::iterator item_end = (*it).second->m_mapTITEM.end();

		while( item_it != item_end )
		{
			if( (*item_it).second->GetItemID() == (WORD)dwItemID )
				dwCount += (*item_it).second->GetCount();

			++item_it;
		}

		++it;
	}

	return dwCount;
}


HRESULT CTItemCraftDlg::Render(DWORD dwTickCount)
{
	if(IsVisible())
	{
		m_pRewardItem[0]->ShowComponent(FALSE);
		m_pRewardArrow[0]->ShowComponent(FALSE);
		m_pRewardBtn[0]->ShowComponent(FALSE);
		m_pCatBtn[0]->ShowComponent(FALSE);
		m_pCatBtnPressed[0]->ShowComponent(FALSE);
	}

	return CTClientUIBase::Render(dwTickCount);
}
