#include "Stdafx.h"
#include "TItemRegBox.h"
#include "TClientGame.h"
#include "Resource.h"

static TComponent* m_pMBase = NULL;
// ====================================================================================================
CTItemRegBox::CTItemRegBox( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
	: CTClientUIBase(pParent, pDesc), m_pItem(NULL)
{
	m_pTitle = FindKid(ID_CTRLINST_TITLE);

	m_pCheckBox[0] = (TButton*) FindKid(27449);
	if (m_pCheckBox[0])
	{
		for (BYTE i = CURRENCY_CREDIT; i < CURRENCY_COUNT; ++i)
		{
			m_pCheckBox[i] = new TButton(this, m_pCheckBox[0]->m_pDESC);
			m_pCheckBox[i]->m_id = GetUniqueID();
			AddKid(m_pCheckBox[i]);
		}
	}

	m_pMBase = FindKid(ID_CTRLINST_MBASE);

	CPoint CheckPos;
	CPoint CheckPos2;
	m_pTitle->GetComponentPos(&CheckPos);
	CheckPos.y += 5;
	CheckPos2 = CheckPos;

	CheckPos.x = m_rc.CenterPoint().x - 100 - m_pCheckBox[1]->m_rc.Width();
	CheckPos2.x = m_rc.CenterPoint().x + 100;

	m_pCheckBox[0]->MoveComponent(CheckPos);
	m_pCheckBox[1]->MoveComponent(CheckPos2);

	CPoint pTitlePos;
	m_pTitle->GetComponentPos(&pTitlePos);
	pTitlePos.y -= 15;
	m_pTitle->MoveComponent(CPoint(pTitlePos.x, pTitlePos.y));

	m_pOK = static_cast<TButton*>(FindKid(ID_CTRLINST_OK));

	TComponent* ButtonDesc[CURRENCY_COUNT] = { NULL };
	for (BYTE i = 0; i < CURRENCY_COUNT; ++i)
	{
		ButtonDesc[i] = new TComponent(this, m_pTitle->m_pDESC);
		ButtonDesc[i]->m_id = GetUniqueID();
		AddKid(ButtonDesc[i]);

		if (i == CURRENCY_MONEY)
		{
			m_pCheckBox[CURRENCY_MONEY]->GetComponentPos(&pTitlePos);
			pTitlePos.x -= 146;
			pTitlePos.y -= m_pCheckBox[CURRENCY_MONEY]->m_rc.Height() / 8;
			ButtonDesc[i]->m_strText = "Money";
		}
		else if (i == CURRENCY_CREDIT)
		{
			m_pCheckBox[CURRENCY_CREDIT]->GetComponentPos(&pTitlePos);
			ButtonDesc[i]->m_strText = "Credits";
			pTitlePos.x -= 77;
			pTitlePos.y -= m_pCheckBox[CURRENCY_CREDIT]->m_rc.Height() / 8;
		}

		ButtonDesc[i]->MoveComponent(pTitlePos);
	}

	TComponent* pTemp;
	
	pTemp = FindKid(ID_CTRLINST_EDIT_COUNT);
	m_pCount = new CTNumberEdit(this, pTemp->m_pDESC, MAX_COUNT_POSCNT);
	
	m_pCredits = new CTNumberEdit(this, pTemp->m_pDESC, TRUNE_LENGTH - 1);
	m_pCredits->m_id = GetUniqueID();
	CPoint pPos;
	m_pCount->GetComponentPos(&pPos);
	pPos.y += 30;
	m_pCredits->MoveComponent(pPos);
	AddKid(m_pCredits);

	RemoveKid(pTemp);
	AddKid(m_pCount);
	delete pTemp;
	
	pTemp = FindKid(ID_CTRLINST_RUNE);
	m_pRune = new CTNumberEdit(this, pTemp->m_pDESC, TRUNE_LENGTH);
	
	RemoveKid(pTemp);
	AddKid(m_pRune);
	delete pTemp;

	pTemp = FindKid(ID_CTRLINST_LUNA);
	m_pLuna = new CTNumberEdit(this, pTemp->m_pDESC, TLUNA_LENGTH);
	
	RemoveKid(pTemp);
	AddKid(m_pLuna);
	delete pTemp;

	pTemp = FindKid(ID_CTRLINST_CRON);
	m_pCron = new CTNumberEdit(this, pTemp->m_pDESC, TCRON_LENGTH);
	
	RemoveKid(pTemp);
	AddKid(m_pCron);
	delete pTemp;

	FindKid(ID_CTRLINST_OK)->m_menu[TNM_LCLICK] = GM_REG_ITEM_PRVSHOP;
	m_nCurrencyIndex = CURRENCY_MONEY;
}
// ----------------------------------------------------------------------------------------------------
CTItemRegBox::~CTItemRegBox()
{
}
// ====================================================================================================
void CTItemRegBox::SetItem(CTClientItem* pItem, BYTE bInven, BYTE bInvenSlot)	
{
	if( pItem )
		m_pTitle->m_strText = pItem->GetTITEM()->m_strNAME;

    m_pItem = pItem;
	m_bInven = bInven;
	m_bInvenSlot = bInvenSlot;

	m_pCount->m_strText = CTChart::Format( TSTR_FMT_NUMBER, pItem->GetCount());
	m_nCurrencyIndex = CURRENCY_MONEY;

	m_pRune->ShowComponent(TRUE);
	m_pLuna->ShowComponent(TRUE);
	m_pCron->ShowComponent(TRUE);	
	m_pMBase->ShowComponent(TRUE);
	m_pCredits->ShowComponent(FALSE);

	DWORD dwRune = 0;
	DWORD dwLuna = 0;
	DWORD dwCron = 0;
	DWORD dwCredits = 0;

	CTClientGame::SplitMoney( pItem->GetPrice(), &dwRune, &dwLuna, &dwCron );
	m_pRune->m_strText = CTChart::Format( TSTR_FMT_NUMBER, dwRune );
	m_pLuna->m_strText = CTChart::Format( TSTR_FMT_NUMBER, dwLuna );
	m_pCron->m_strText = CTChart::Format( TSTR_FMT_NUMBER, dwCron );
	m_pCredits->m_strText.Empty();

	m_pCount->MoveCaretToFront();
    SwitchFocus(m_pCount);
}
// ====================================================================================================
void CTItemRegBox::SetCount(DWORD dwCnt)
{
	CString str;
	str = CTChart::Format( TSTR_FMT_NUMBER, dwCnt);
	m_pCount->SetText(str);
}
// ----------------------------------------------------------------------------------------------------
void CTItemRegBox::SetRune(DWORD dwRune)
{
	CString str;
	str = CTChart::Format( TSTR_FMT_NUMBER, dwRune);
	m_pCount->SetText(str);
}
// ----------------------------------------------------------------------------------------------------
void CTItemRegBox::SetLuna(DWORD dwLuna)
{
	CString str;
	str = CTChart::Format( TSTR_FMT_NUMBER, dwLuna);
	m_pCount->SetText(str);
}
// ----------------------------------------------------------------------------------------------------
void CTItemRegBox::SetCron(DWORD dwCron)
{
	CString str;
	str = CTChart::Format( TSTR_FMT_NUMBER, dwCron);
	m_pCount->SetText(str);
}
// ----------------------------------------------------------------------------------------------------
void CTItemRegBox::SetOkGM( DWORD dwGM )
{
	if( m_pOK )
	{
		m_pOK->m_menu[ TNM_LCLICK ] = dwGM;
	}
}
// ====================================================================================================
DWORD CTItemRegBox::GetCount() const
{
	return (DWORD)(::atoi(m_pCount->m_strText));
}
// ----------------------------------------------------------------------------------------------------
DWORD CTItemRegBox::GetRune() const
{
	return (DWORD)(::atoi(m_pRune->m_strText));
}
// ----------------------------------------------------------------------------------------------------
DWORD CTItemRegBox::GetLuna() const
{
	return (DWORD)(::atoi(m_pLuna->m_strText));
}
// ----------------------------------------------------------------------------------------------------
DWORD CTItemRegBox::GetCron() const
{
	return (DWORD)(::atoi(m_pCron->m_strText));
}

DWORD CTItemRegBox::GetCredits() const
{
	if (m_nCurrencyIndex == CURRENCY_CREDIT)
		return (DWORD)(atoi(m_pCredits->m_strText));
	else
		return 0;
}

INT CTItemRegBox::GetCurrencyIndex() const
{
	return m_nCurrencyIndex;
}
// ====================================================================================================
TEdit* CTItemRegBox::GetCurEdit()
{
	if( !IsVisible() || !GetFocus() )
		return NULL;

	if( m_pCount->GetFocus() )
		return m_pCount;
	if( m_pRune->GetFocus() )
		return m_pRune;
	if( m_pLuna->GetFocus() )
		return m_pLuna;
	if( m_pCron->GetFocus() )
		return m_pCron;
	if (m_pCredits->GetFocus())
		return m_pCredits;

	return NULL;
}
// ====================================================================================================
BOOL CTItemRegBox::CanWithItemUI()
{
	return TRUE;
}
// ====================================================================================================

HRESULT CTItemRegBox::Render(DWORD dwTickCount)
{
	if (m_bVisible)
	{
		if (m_nCurrencyIndex != CURRENCY_COUNT)
		{
			m_pCheckBox[m_nCurrencyIndex]->Select(TRUE);

			if (m_nCurrencyIndex == CURRENCY_MONEY)
			{
				m_pRune->ShowComponent(TRUE);
				m_pLuna->ShowComponent(TRUE);
				m_pCron->ShowComponent(TRUE);

				m_pCredits->ShowComponent(FALSE);
				m_pMBase->ShowComponent(TRUE);
			}
			else if (m_nCurrencyIndex == CURRENCY_CREDIT)
			{
				m_pRune->ShowComponent(FALSE);
				m_pLuna->ShowComponent(FALSE);
				m_pCron->ShowComponent(FALSE);

				m_pCredits->ShowComponent(TRUE);
				m_pMBase->ShowComponent(FALSE);
			}

			if (m_nCurrencyIndex == CURRENCY_MONEY)
				m_pCheckBox[CURRENCY_CREDIT]->Select(FALSE);
			else
				m_pCheckBox[CURRENCY_MONEY]->Select(FALSE);
		}
	}

	return CTClientUIBase::Render(dwTickCount);
}

void CTItemRegBox::OnLButtonUp(UINT nFlags, CPoint pt)
{
	for (BYTE i = 0; i < CURRENCY_COUNT; ++i)
	{
		if (m_pCheckBox[i]->HitTest(pt))
		{
			if(m_pCheckBox[i == CURRENCY_MONEY ? CURRENCY_CREDIT : CURRENCY_MONEY]->m_bState & TBUTTON_STATE_DOWN)
			{
				if (i == CURRENCY_CREDIT)
				{
					m_pCredits->m_strText.Empty();
					SwitchFocus(m_pCredits);
				}

				m_nCurrencyIndex = i;
				break;
			}
		}
	}

	return CTClientUIBase::OnLButtonUp(nFlags, pt);
}