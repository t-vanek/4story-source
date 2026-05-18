#include "stdafx.h"
#include "TBowRegisterWaitingDlg.h"
#include "TClientGame.h"
#include "TClientWnd.h"

CTBowRegisterWaitingDlg::CTBowRegisterWaitingDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	static DWORD dwToHide[] = { 
		26973,
		27462,
		27463
	};

	for (BYTE i = 0; i < sizeof(dwToHide) / sizeof(DWORD); ++i)
	{
		TComponent* pComponent = FindKid(dwToHide[i]);
		if(pComponent)
		{
			RemoveKid(pComponent);
			delete pComponent;
		}
	}

	FindKid(ID_CTRLINST_CLOSE)->m_menu[TNM_LCLICK] = GM_CANCEL_BOW_QUEUE;
	FindKid(ID_CTRLINST_TITLE)->m_strText = "Battle of the Worlds";

	for (BYTE i = 0; i < 2; ++i)
	{
		m_pMinute[i] = (TImageList*) FindKid(26970 + i);
		m_pSecond[i] = (TImageList*) FindKid(26968 + i);
	}

	SetTime(0, 0);

	auto MainFrame = FindKid(25803);
	for (auto& Kids : m_kids)
	{
		if (Kids->m_id == MainFrame->m_id)
			continue;

		if (Kids->m_strText != NAME_NULL)
			Kids->MoveComponentBy(-97, -2);
		else if (Kids->m_bType == TCML_TYPE_BUTTON)
			Kids->MoveComponentBy(-239, -3);
		else
			Kids->MoveComponentBy(0, -5);

		if (Kids->m_strText != NAME_NULL || Kids->m_bType == TCML_TYPE_BUTTON)
			continue;

		Kids->m_style |= TS_CUSTOM_COLOR;
		Kids->m_dwColor = 0xBBFFFFFF;

		if (Kids->m_bType == TCML_TYPE_IMAGELIST)
		{
			auto ImageList = (TImageList*)Kids;
			ImageList->EnableUserColor(TRUE);
			ImageList->SetUserColor(0xDDFFFFFF);
		}
	}

	for (auto& Kids : m_kids) //Second UI change
	{
		if (Kids->m_id == MainFrame->m_id)
			continue;

		Kids->MoveComponentBy(10, 10);

		if (Kids->m_strText == NAME_NULL && Kids->m_bType != TCML_TYPE_BUTTON)
			Kids->MoveComponentBy(0, 2);
	}
}

CTBowRegisterWaitingDlg::~CTBowRegisterWaitingDlg()
{

}

BOOL CTBowRegisterWaitingDlg::CanWithItemUI()
{
	return TRUE;
}

void CTBowRegisterWaitingDlg::SetTime(BYTE bMin, BYTE bSec)
{
	BYTE bMin1 = bMin / 10;
	BYTE bMin2 = bMin % 10;

	BYTE bSec1 = bSec / 10;
	BYTE bSec2 = bSec % 10;

	if (bMin <= 10)
	{
		m_pMinute[1]->SetCurImage(0);
		m_pMinute[0]->SetCurImage(bMin);
	}
	if (bSec <= 10)
	{
		m_pSecond[1]->SetCurImage(0);
		m_pSecond[0]->SetCurImage(bSec);
	}
	if (bMin >= 10)
	{
		m_pMinute[1]->SetCurImage(bMin1);
		m_pMinute[0]->SetCurImage(bMin2);
	}
	if (bSec >= 10)
	{
		m_pSecond[1]->SetCurImage(bSec1);
		m_pSecond[0]->SetCurImage(bSec2);
	}
}

void CTBowRegisterWaitingDlg::ShowComponent(BOOL bVisible)
{
	if (!m_bVisible && bVisible)
		MoveComponent(CPoint(
			CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_MIDDLE].x - 370 + 186 - 53, //I really hope this source wont get leaked :d
			CTClientUIBase::m_vBasis[TBASISPOINT_CENTER_MIDDLE].y - 250));

	auto MainFrame = FindKid(25803);
	MainFrame->UseOwnImages(61001);
	MainFrame->m_style |= TS_CUSTOM_COLOR;
	MainFrame->m_dwColor = 0xAA000000;

	CTClientUIBase::ShowComponent(bVisible);
}