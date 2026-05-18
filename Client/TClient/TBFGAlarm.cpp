#include "stdafx.h"
#include "TBFGAlarm.h"
#include "TClientGame.h"

#define ID_BOW_BUTTON				(0x00006DCB)
#define ID_BOW_TEXT				(0x00006DCC)
#define ID_BOW_TIME				(0x00006DCD)

CTBFGAlarm::CTBFGAlarm( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	m_pAlarm = static_cast<TButton*>(FindKid(ID_BOW_BUTTON));

	m_pText = FindKid(ID_BOW_TEXT);
	m_pTime = FindKid(ID_BOW_TIME);

	m_pText->m_strText = "Battle of the Worlds will soon start.\n<< Click here to register.";
	m_pText->MoveComponentBy(3, 11);
	m_pAlarm->m_menu[TNM_LCLICK] = GM_OPEN_REGISTER_UI;

	m_dwDefaultTextColor = m_pText->m_pFont->m_dwColor;

	m_bType = SYSTEM_BOW;
	m_bBRType = BR_SOLO;

	m_bMovingForward = FALSE;
	m_bMovingBackward = FALSE;

	m_pTime->MoveComponentBy(-2, 0);
}

CTBFGAlarm::~CTBFGAlarm()
{

}

BOOL CTBFGAlarm::CanWithItemUI()
{
	return TRUE;
}

void CTBFGAlarm::SetType(BYTE bType, BYTE bBRType)
{
	m_bType = bType;
	m_bBRType = bBRType;

	switch (bType)
	{
	case SYSTEM_BOW:
		m_pText->m_strText = "Battle of the Worlds";
		break;
	case SYSTEM_BR:

	{
		if (bBRType == BR_SOLO)



			m_pText->m_strText = "Battle Royale (Solo)";
		else
			m_pText->m_strText = "Battle Royale (Team)";
	}
	break;
	}

	UseOwnImages(61001);
	m_style |= TS_CUSTOM_COLOR;
	m_dwColor = 0xAA000000;
}

HRESULT CTBFGAlarm::Render(DWORD dwTickCount)
{
	if (!IsVisible())
		return CTClientUIBase::Render(dwTickCount);

	static FLOAT TimeMove = 0.0f;
	CPoint pt = CTClientUIBase::m_vBasis[TBASISPOINT_RIGHT_TOP];
	pt.x -= 42;

	if (m_bMovingForward)
	{
		FLOAT PosMax = FLOAT(dwTickCount / 2);
		if (PosMax < 4.0f)
			PosMax = 4.0f;

		MoveComponentBy(-INT(PosMax / 2.0f), 0);
		m_pAlarm->MoveComponentBy(INT(PosMax / 2.0f), 0);
		m_pTime->MoveComponentBy(INT(PosMax / 4.0f), 0);

		TimeMove += PosMax / 2.0f;
		if (TimeMove >= 12) { //12px difference in the image
			m_pTime->MoveComponentBy(1, 0);
			TimeMove = 0.0f;
		}

	}

	if (m_rc.left < pt.x - 180)
		m_bMovingForward = FALSE;

	return CTClientUIBase::Render(dwTickCount);
}

void CTBFGAlarm::OnMouseMove(UINT nFlags, CPoint pt)
{

}

void CTBFGAlarm::SetTextColor(BYTE bStatus, BYTE bBRType)
{
	switch (m_bType)
	{
	case SYSTEM_BOW:
		{
			if (bStatus == BS_ALARM)
			{
				m_pText->SetTextClr(m_dwDefaultTextColor);
				m_pTime->SetTextClr(m_dwDefaultTextColor);
			}
			else if(bStatus == BS_PEACE)
			{
				m_pText->SetTextClr(0xFFCC0000);
				m_pTime->SetTextClr(0xFFCC0000);
			}
		}
		break;
	case SYSTEM_BR:
		{
			switch(bBRType)
			{
			case BR_SOLO:
				{
					m_pText->SetTextClr(0xFF00F210);
					m_pTime->SetTextClr(0xFF00F210);
				}
				break;
			case BR_TEAM:
				{
					m_pText->SetTextClr(0xFF00B3FF);
					m_pTime->SetTextClr(0xFF00B3FF);
				}
				break;
			}
		}
		break;
	}

	DWORD Color = m_pText->m_pFont->m_dwColor;
	m_pAlarm->m_style |= TS_CUSTOM_COLOR;
	m_pAlarm->m_dwColor = Color;
	m_pAlarm->m_pUp->m_style |= TS_CUSTOM_COLOR;
	m_pAlarm->m_pUp->m_dwColor = Color;
}

void CTBFGAlarm::ResetPosition()
{
	if (m_bVisible) {
		m_bVisible = FALSE;
		ShowComponent(TRUE);
	}
}

void CTBFGAlarm::ShowComponent(BOOL Visible)
{
	if (!m_bVisible && Visible) {
		CPoint pt = CTClientUIBase::m_vBasis[TBASISPOINT_RIGHT_TOP];
		pt.x -= 42;
		pt.y += 80;
		MoveComponent(pt);

		m_pAlarm->MoveComponent(CPoint(0, 13));
		m_pTime->MoveComponent(CPoint(-2, 48));

		PopUp();
	}
	else if (!Visible)
		m_bMovingForward = FALSE;

	return CTClientUIBase::ShowComponent(Visible);
}

void CTBFGAlarm::SetTime(DWORD dwSecond)
{
	BYTE bMin = BYTE(dwSecond / 60);
	BYTE bSec = BYTE(dwSecond % 60);

	m_pTime->m_strText.Format("%02d:%02d", bMin, bSec);
}

void CTBFGAlarm::PopUp()
{
	m_bMovingForward = TRUE;
}