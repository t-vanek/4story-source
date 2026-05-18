#include "StdAfx.h"
#include "TSecuritySystemDlg.h"
#include "TClientGame.h"
#include "TCustomStrings.h"

CTSecuritySystemDlg::CTSecuritySystemDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
	: CTClientUIBase(pParent, pDesc)
{
	m_pDlgTitle = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_DLGTITLE));
	m_pTitle = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_TITLE));
	m_pHint = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_HINT));
	m_pBlueTxt = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_BTXT));
	m_pYellowTxt = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_YTXT));
	m_pShowText = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_CHECKTXT));
	m_pStrCode1 = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_CODE1TXT));
	m_pStrCode2 = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_CODE2TXT));
	m_pStrCode3 = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_CODE3TXT));

	m_pMiddleBtn = static_cast<TButton *>(FindKid(ID_CTRLINST_SC_MIDDLEBTN));
	m_pMiddleBtn2 = static_cast<TButton *>(FindKid(ID_CTRLINST_SC_MIDDLEBTN2));
	m_pRightBtn = static_cast<TButton *>(FindKid(ID_CTRLINST_SC_RIGHTBTN));
	m_pLeftBtn = static_cast<TButton *>(FindKid(ID_CTRLINST_SC_LEFTBTN));

	m_pTextBox1 = static_cast<TEdit *>(FindKid(ID_CTRLINST_SC_OLDCODEBOX));
	m_pTextBox2 = static_cast<TEdit *>(FindKid(ID_CTRLINST_SC_ENTERCODEBOX));
	m_pTextBox3 = static_cast<TEdit *>(FindKid(ID_CTRLINST_SC_CONFIRMCODEBOX));

	m_pTextBox1BackImg = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_TXTBOX1BACKIMG));
	m_pTextBox2BackImg = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_TXTBOX2BACKIMG));
	m_pTextBox3BackImg = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_TXTBOX3BACKIMG));

	m_pCheckBox = static_cast<TComponent *>(FindKid(ID_CTRLINST_SC_CHECKBOX));
	
	TComponent* pCloseBtn = FindKid(ID_CTRLINST_CLOSE);
	pCloseBtn->m_menu[TNM_LCLICK] = GM_CLOSE_UI;

	m_pLeftBtn->m_menu[TNM_LCLICK] = GM_SC_LEFTBTN;

	m_pMiddleBtn->m_menu[TNM_LCLICK] = GM_SC_MIDDLEBTN;
	m_pRightBtn->m_menu[TNM_LCLICK] = GM_SC_RIGHTBTN;

	m_bPreDlgType = TDLG_SC_NONE;
	m_bWrongAttempts = 0;
}

CTSecuritySystemDlg::~CTSecuritySystemDlg()
{
}

BYTE CTSecuritySystemDlg::SwitchMode(BYTE bDlgType, BYTE bOpenType)
{
	if(bDlgType > TDLG_SC_COUNT)
		return TERR_NONE;

	SetOpenType(bOpenType);
	if (bDlgType != m_bDlgType)
		m_bPreDlgType = m_bDlgType;
	SetDlgType(bDlgType);

	switch(bDlgType)
	{
	case TDLG_SC_MCREATE:
		DisplayCreateMode();
		break;
	case TDLG_SC_MCHANGE:
		DisplayChangeMode();
		break;
	case TDLG_SC_MDISABLE:
		DisplayDisableMode();
		break;
	case TDLG_SC_MDISABLED:
		DisplayDisabledMode();
		break;
	case TDLG_SC_MWRONGCODE:
		DisplayWrongCode();
		break;
	case TDLG_SC_MENABLE:
		DisplayEnableMode();
		break;
	case TDLG_SC_MUNLOCK:
		DisplayUnlockMode();
		break;
	case TDLG_SC_MUNLOCKED:
		DisplayUnlockedMode();
		break;
	case TDLG_SC_MCHANGED:
		DisplayChangedMode();
		break;
	case TDLG_SC_TEMPLOCKED:
		DisplayTempLockedMode();
		break;
	}

	return TERR_NONE;
}

void CTSecuritySystemDlg::PrepareDisplay()
{
	bTitleShow = FALSE;
	bHintShow = FALSE;
	bBlueTextShow = FALSE;
	bYellowTextShow = FALSE;
	bCheckBoxTxtShow = FALSE;
	bstrCode1Show = FALSE;
	bstrCode2Show = FALSE;
	bstrCode3Show = FALSE;
	bMiddleBtnShow = FALSE;
	bMiddleBtn2Show = FALSE;
	bRightBtnShow = FALSE;
	bLeftBtnShow = FALSE;
	bTextBox1Show = FALSE;
	bTextBox2Show = FALSE;
	bTextBox3Show = FALSE;
	bCheckBoxShow = FALSE;

	m_pMiddleBtn->EnableComponent(FALSE);
	m_pMiddleBtn2->EnableComponent(FALSE);
	m_pRightBtn->EnableComponent(FALSE);
	m_pLeftBtn->EnableComponent(FALSE);

	m_pTextBox1->ClearText();
	m_pTextBox2->ClearText();
	m_pTextBox3->ClearText();

	m_bSessionType = 0;
	m_dwAdviseTick = 0;

}

TEdit* CTSecuritySystemDlg::GetTextBoxContent(BYTE bBox) //What a fucking retarded implementation
{
	if( !IsVisible() )
		return NULL;

	if( !bTextBox1Show && !bTextBox2Show && !bTextBox3Show )
		return NULL;

	if(!bBox)
	{
		if( m_pTextBox1->CanProcess() && m_pFocus == m_pTextBox1 )
			return m_pTextBox1;
		if( m_pTextBox2->CanProcess() && m_pFocus == m_pTextBox2 )
			return m_pTextBox2;
		if( m_pTextBox3->CanProcess() && m_pFocus == m_pTextBox3 )
			return m_pTextBox3;
	}
	else
	{
		switch(bBox)
		{
		case TBOX_SC_OLDCODE:
			{
				if(bTextBox1Show)
					return m_pTextBox1;
			}
			break;
		case TBOX_SC_NEWCODE:
			{
				if(bTextBox2Show)
					return m_pTextBox2;
			}
			break;
		case TBOX_SC_CONFIRMCODE:
			{
				if(bTextBox3Show)
					return m_pTextBox3;
			}
			break;
		}
	}
	return NULL;
}

void CTSecuritySystemDlg::GetTextBoxString(BYTE bID, CString& Output)
{
	switch (bID)
	{
	case 1:
		Output = m_pTextBox1->m_strText;
		break;
	case 2:
		Output = m_pTextBox2->m_strText;
		break;
	case 3:
		Output = m_pTextBox3->m_strText;
		break;
	default:
		Output = NAME_NULL;
	}
}

BOOL CTSecuritySystemDlg::CheckCode(LPCSTR str)
{
	int l = int(strlen(str));
	unsigned char* e = (unsigned char *)(str+l);
	unsigned char* p = (unsigned char *)str;
	BOOL bIncludeSpecialChar = FALSE;
	CString strC = str;

	while(p < e)
	{
		if('0' <= *p && *p <= '9') 
			++p;				
		else if('a' <= *p && *p <= 'z') 
			++p;	
		else if('A' <= *p && *p <= 'Z') 
			++p;
		else if( (*p == 220) ||
				(*p == 214) ||
				(*p == 196) ||
				(*p == 252) ||
				(*p == 246) ||
				(*p == 228) ||
				(*p == 223) )
			++p;
		else
		{
			bIncludeSpecialChar =  TRUE;
			break;
		}
	}

	if(bIncludeSpecialChar)
	{	
		for( int i=0; i < strC.GetLength(); ++i ) 
		{ 
			if(isdigit( str[i] )) 
				return TRUE;	
		}
	}
	return FALSE;
}

void CTSecuritySystemDlg::RenderAdvise()
{
	if(!IsVisible())
		return;

	if(!m_bSessionType)
		return;

	CString strSecureCode;
	CString strConfirmCode;
	BYTE bSecureCodeLength;

	switch(m_bDlgType)
	{
	case TDLG_SC_MCREATE:
	case TDLG_SC_MCHANGE:
		{
			TEdit* pEditSecure = GetTextBoxContent(TBOX_SC_NEWCODE);
			TEdit* pEditConfirm = GetTextBoxContent(TBOX_SC_CONFIRMCODE);

			if(!pEditSecure)
				return;

			if(!pEditConfirm)
				return;

			m_pMiddleBtn->EnableComponent(FALSE);

			if(!pEditSecure->m_strText.IsEmpty())
			{
				strSecureCode = pEditSecure->m_strText;
				strConfirmCode = pEditConfirm->m_strText;
				bSecureCodeLength = strSecureCode.GetLength();

				if(bSecureCodeLength >= 8 && bSecureCodeLength <= 12)
				{
					if(CheckCode(strSecureCode))
					{
						m_pYellowTxt->m_strText = TSTR_SC_CODEVALID;

						if( strSecureCode == strConfirmCode )
						{
							m_pYellowTxt->m_strText = TSTR_SC_CODEVALID;
							m_pMiddleBtn->EnableComponent(TRUE);
						}
						else
							m_pYellowTxt->m_strText = TSTR_SC_CODENOMATCH;
					}
					else
					{
						m_pYellowTxt->m_strText = TSTR_SC_CODENOSPECIAL;
					}
				}
				else if(bSecureCodeLength < 8)
					m_pYellowTxt->m_strText = TSTR_SC_CODETOOSHORT;
				else if(bSecureCodeLength > 12)
					m_pYellowTxt->m_strText = TSTR_SC_CODETOOLONG;
				else
					m_pYellowTxt->m_strText = TSTR_SC_CODELENGTH;
			}
		}
		break;
	}
}

void CTSecuritySystemDlg::DisplayCreateMode()
{
	m_bSessionType = TSESSION_SC_WRITE;

	bMiddleBtnShow = TRUE;
	bTitleShow = TRUE;
	bHintShow = TRUE;
	bstrCode2Show = TRUE;
	bstrCode3Show = TRUE;
	bTextBox2Show = TRUE;
	bTextBox3Show = TRUE;
	bCheckBoxTxtShow = FALSE;
	bCheckBoxShow = FALSE;
	bBlueTextShow = TRUE;
	bYellowTextShow = TRUE;

	m_pMiddleBtn->EnableComponent(TRUE);
	
	m_pDlgTitle->m_strText = TSTR_SC_CREATE;
	m_pTitle->m_strText = TSTR_SC_ADVISETITLE;							
	m_pHint->m_strText = TSTR_SC_ADVISE;
	m_pBlueTxt->m_strText = TSTR_SC_HINT;
	m_pMiddleBtn->m_strText = TSTR_SC_CREATE;
	m_pStrCode2->m_strText = TSTR_SC_ENTERCODE;
	m_pStrCode3->m_strText = TSTR_SC_CONFIRMCODE;
	m_pShowText->m_strText = TSTR_SC_SHOWAGAIN;
	m_pYellowTxt->m_strText = TSTR_SC_CODELENGTH;

	CPoint pt;
	m_pShowText->GetComponentPos(&pt);
	pt.y += 20;
	m_pShowText->MoveComponent(pt);
	m_pCheckBox->GetComponentPos(&pt);
	pt.y += 20;
	m_pCheckBox->MoveComponent(pt);
}

void CTSecuritySystemDlg::DisplayEnableMode()
{
	m_bSessionType = TSESSION_SC_NORMAL;

	bMiddleBtnShow = TRUE;
	bTitleShow = TRUE;
	bHintShow = TRUE;
	bBlueTextShow = TRUE;
	bYellowTextShow = TRUE;

	m_pMiddleBtn->EnableComponent(TRUE);
	
	m_pDlgTitle->m_strText = TSTR_SC_CREATESUCCESSDLGTITLE;
	m_pTitle->m_strText = TSTR_SC_CREATESUCCESSTITLE;							
	m_pHint->m_strText = TSTR_SC_CREATESUCCESSHINT;
	m_pBlueTxt->m_strText = TSTR_SC_CREATESUCCESSADVISE;
	m_pYellowTxt->m_strText = TSTR_SC_CREATESUCCESSADDASK;
	m_pMiddleBtn->m_strText = TSTR_SC_CREATESUCCESSENABLEBTN;
}

void CTSecuritySystemDlg::DisplayDisableMode()
{
	m_bSessionType = TSESSION_SC_WRITE;

	bTitleShow = TRUE;
	bHintShow = TRUE;
	bBlueTextShow = TRUE;
	bYellowTextShow = TRUE;
	bstrCode3Show = TRUE;
	bTextBox3Show = TRUE;
	bMiddleBtnShow = TRUE;
	
	m_pMiddleBtn->EnableComponent(TRUE);

	m_pDlgTitle->m_strText = TSTR_SC_DISABLEDDLGTITLE;
	m_pTitle->m_strText = TSTR_SC_DISABLEDDLGTITLE;			

	m_pHint->m_strText = TSTR_SC_DISABLEADVISE;
	m_pBlueTxt->m_strText = TSTR_SC_DISABLEHINT;
	m_pYellowTxt->m_strText = TSTR_SC_HINT;
	m_pStrCode3->m_strText = TSTR_SC_ENTERCODE;
	m_pMiddleBtn->m_strText = TSTR_SC_DISABLEBTN;
}

void CTSecuritySystemDlg::DisplayDisabledMode()
{
	m_bSessionType = TSESSION_SC_NORMAL;

	bMiddleBtnShow = TRUE;
	bTitleShow = TRUE;
	bHintShow = TRUE;

	m_pMiddleBtn->EnableComponent(TRUE);

	m_pDlgTitle->m_strText = TSTR_SC_DISABLEDDLGTITLE;
	m_pTitle->m_strText = TSTR_SC_DISABLEDTITLE;							
	m_pHint->m_strText = TSTR_SC_DISABLEBTNHINT;
	m_pMiddleBtn->m_strText = TSTR_SC_DISABLEDBTN;
}

void CTSecuritySystemDlg::DisplayChangeMode()
{
	m_bSessionType = TSESSION_SC_WRITE;

	bMiddleBtnShow = TRUE;
	bTitleShow = TRUE;
	bHintShow = TRUE;
	bstrCode1Show = TRUE;
	bstrCode2Show = TRUE;
	bstrCode3Show = TRUE;
	bTextBox1Show = TRUE;
	bTextBox2Show = TRUE;
	bTextBox3Show = TRUE;
	bCheckBoxTxtShow = FALSE;
	bCheckBoxShow = FALSE;
	bBlueTextShow = TRUE;
	bYellowTextShow = TRUE;

	m_pMiddleBtn->EnableComponent(TRUE);
	
	m_pDlgTitle->m_strText = TSTR_SC_CHANGETITLE;
	m_pTitle->m_strText = TSTR_SC_CHANGETITLE;							
	m_pHint->m_strText = TSTR_SC_CHANGEADVISE;
	m_pBlueTxt->m_strText = TSTR_SC_HINT;
	m_pYellowTxt->m_strText = TSTR_SC_CODELENGTH;
	m_pStrCode1->m_strText = TSTR_SC_CHANGECURCODE;
	m_pStrCode2->m_strText = TSTR_SC_CHANGENEWCODE;
	m_pStrCode3->m_strText = TSTR_SC_CONFIRMCODE;
	m_pMiddleBtn->m_strText = TSTR_SC_CHANGEBTN;
}

void CTSecuritySystemDlg::DisplayWrongCode()
{
	m_bSessionType = TSESSION_SC_WRITE;

	bTitleShow = TRUE;
	bHintShow = TRUE;
	bBlueTextShow = TRUE;
	bYellowTextShow = TRUE;
	bstrCode3Show = TRUE;
	bTextBox3Show = TRUE;
	bLeftBtnShow = TRUE;
	bRightBtnShow = TRUE;

	m_pRightBtn->EnableComponent(TRUE);
	m_pLeftBtn->EnableComponent(TRUE);

	switch(GetPreDlgType())
	{
	case TDLG_SC_MCHANGE:
		{
			m_pDlgTitle->m_strText = TSTR_SC_CHANGETITLE;
			m_pLeftBtn->m_strText = TSTR_SC_CHANGEBTN;
		}
		break;
	case TDLG_SC_MDISABLE:
		{
			m_pDlgTitle->m_strText = TSTR_SC_DISABLEDDLGTITLE;
			m_pLeftBtn->m_strText = TSTR_SC_DISABLEBTN;
		}
		break;
	case TDLG_SC_MUNLOCK:
		{
			m_pDlgTitle->m_strText = TSTR_SC_UNLOCKTITLE;
			m_pLeftBtn->m_strText = TSTR_SC_UNLOCKBTNTXT;
		}
		break;
	default:
		{
			m_pDlgTitle->m_strText = TSTR_SC_WRONGTITLE;
			m_pLeftBtn->m_strText = TSTR_SC_WRONGLEFTBTN;
		}
		break;
	}

	CString Adolf;
	Adolf.Format(TSTR_SC_WRONGFAILS,m_bWrongAttempts);
	
	m_pTitle->m_strText = TSTR_SC_WRONGTITLE;							
	m_pHint->m_strText = TSTR_SC_WRONGADVISE;
	m_pBlueTxt->m_strText = TSTR_SC_WRONGHINT;
	m_pYellowTxt->m_strText = Adolf;
	m_pStrCode3->m_strText = TSTR_SC_ENTERCODE; 
	m_pRightBtn->m_strText = TSTR_SC_WRONGRESET;
}

void CTSecuritySystemDlg::DisplayUnlockMode()
{
	m_bSessionType = TSESSION_SC_WRITE;

	bMiddleBtnShow = TRUE;
	bTitleShow = TRUE;
	bHintShow = TRUE;
	bstrCode2Show = TRUE;
	bTextBox2Show = TRUE;

	m_pMiddleBtn->EnableComponent(TRUE);

	m_pDlgTitle->m_strText = TSTR_SC_UNLOCKTITLE;
	m_pTitle->m_strText = TSTR_SC_UNLOCKTITLE;							
	m_pHint->m_strText = TSTR_SC_UNLOCKADVISE;
	m_pMiddleBtn->m_strText = TSTR_SC_UNLOCKBTNTXT;
	m_pStrCode2->m_strText = TSTR_SC_UNLOCKTITLE;
}

void CTSecuritySystemDlg::DisplayUnlockedMode()
{
	m_bSessionType = TSESSION_SC_NORMAL;

	bTitleShow = TRUE;
	bHintShow = TRUE;
	bBlueTextShow = TRUE;
	bYellowTextShow = TRUE;
	bMiddleBtnShow = TRUE;
	
	m_pMiddleBtn->EnableComponent(TRUE);

	m_pDlgTitle->m_strText = TSTR_SC_UNLOCKTITLE;
	m_pTitle->m_strText = TSTR_SC_UNLOCKEDTITLE;							
	m_pHint->m_strText = TSTR_SC_UNLOCKEDADVISE;
	m_pBlueTxt->m_strText = TSTR_SC_UNLOCKEDHINT;
	m_pYellowTxt->m_strText = TSTR_SC_UNLOCKEDRELOGHINT;
	m_pMiddleBtn->m_strText = TSTR_SC_UNLOCKEDCLOSE;
}

void CTSecuritySystemDlg::DisplayChangedMode()
{
	m_bSessionType = TSESSION_SC_NORMAL;

	bTitleShow = TRUE;
	bHintShow = TRUE;
	bBlueTextShow = TRUE;
	bMiddleBtnShow = TRUE;
	
	m_pMiddleBtn->EnableComponent(TRUE);

	m_pDlgTitle->m_strText = TSTR_SC_CHANGETITLE;
	m_pTitle->m_strText = TSTR_SC_CHANGESUCCESS;							
	m_pHint->m_strText = TSTR_SC_CODECHANGED;
	m_pBlueTxt->m_strText = TSTR_SC_NEWCODEADVIsE;
	m_pMiddleBtn->m_strText = TSTR_SC_DISABLEDBTN;

}
void CTSecuritySystemDlg::DisplayTempLockedMode()
{
	m_bSessionType = TSESSION_SC_NORMAL;

	bTitleShow = TRUE;
	bBlueTextShow = TRUE;
	bMiddleBtnShow = TRUE;
	
	m_pMiddleBtn->EnableComponent(TRUE);

	CString strLockTime;
	strLockTime.Format(TSTR_SC_FAILEDTOOOFTEN, m_bLockHours, m_bLockMinutes);

	m_pDlgTitle->m_strText = TSTR_SC_UNLOCKTITLE;
	m_pTitle->m_strText = TSTR_SC_WRONGTITLE;							
	m_pBlueTxt->m_strText = strLockTime;
	m_pMiddleBtn->m_strText = TSTR_SC_DISABLEDBTN;
}

void CTSecuritySystemDlg::SetDlgType(BYTE bDlgType)
{
	m_bDlgType = bDlgType;
}

void CTSecuritySystemDlg::SetOpenType(BYTE bOpenType)
{
	m_bOpenType = bOpenType;
}

void CTSecuritySystemDlg::SetWrongAttempts(BYTE bWrongAttempts)
{
	m_bWrongAttempts = bWrongAttempts;
}

void CTSecuritySystemDlg::SetEnabled(BOOL bEnabled)
{
	m_bEnabled = bEnabled;
}

void CTSecuritySystemDlg::SetExist(BOOL bExist)
{
	m_bExist = bExist;
}

void CTSecuritySystemDlg::SetLockTime(BYTE bHour, BYTE bMinute)
{
	m_bLockHours = bHour;
	m_bLockMinutes = bMinute;
}

void CTSecuritySystemDlg::SetLocked(BYTE bLock)
{
	m_bLocked = bLock;
}

HRESULT CTSecuritySystemDlg::Render(DWORD dwTickCount)
{
	if (CTClientGame::IsInBOWMap() || CTClientGame::IsInBRMap() && m_bVisible)
		ShowComponent(FALSE);

	m_pTitle->ShowComponent(bTitleShow);
	m_pHint->ShowComponent(bHintShow);
	m_pBlueTxt->ShowComponent(bBlueTextShow);
	m_pYellowTxt->ShowComponent(bYellowTextShow);
	m_pShowText->ShowComponent(bCheckBoxTxtShow);
	m_pStrCode1->ShowComponent(bstrCode1Show);
	m_pStrCode2->ShowComponent(bstrCode2Show);
	m_pStrCode3->ShowComponent(bstrCode3Show);
	m_pMiddleBtn->ShowComponent(bMiddleBtnShow);
	m_pMiddleBtn2->ShowComponent(bMiddleBtn2Show);
	m_pRightBtn->ShowComponent(bRightBtnShow);
	m_pLeftBtn->ShowComponent(bLeftBtnShow);
	m_pTextBox1->ShowComponent(bTextBox1Show);
	m_pTextBox2->ShowComponent(bTextBox2Show);
	m_pTextBox3->ShowComponent(bTextBox3Show);
	m_pTextBox1BackImg->ShowComponent(bTextBox1Show);
	m_pTextBox2BackImg->ShowComponent(bTextBox2Show);
	m_pTextBox3BackImg->ShowComponent(bTextBox3Show);
	m_pCheckBox->ShowComponent(bCheckBoxShow);

	if( (m_bSessionType) && ( (m_bDlgType == TDLG_SC_MCREATE) || (m_bDlgType == TDLG_SC_MCHANGE) ) )
	{
		if(m_dwAdviseTick > 10)
		{
			RenderAdvise();
			m_dwAdviseTick = 0;
		}
		m_dwAdviseTick += 1;
	}

	if(m_bOpenType == 1 && m_bDlgType == TDLG_SC_MCREATE)
	{
		/*	bCheckBoxTxtShow = TRUE;
		bCheckBoxShow = TRUE;*/
	}

	if(m_bDlgType == TDLG_SC_MWRONGCODE)
	{
		CString Adolf;
		Adolf.Format(TSTR_SC_WRONGFAILS,m_bWrongAttempts);
		m_pYellowTxt->m_strText = Adolf;
	}

	return CTClientUIBase::Render(dwTickCount);
}

void CTSecuritySystemDlg::OnLButtonDown(UINT nFlags, CPoint pt)
{
	if(!CanProcess()) 
		return;
	
	if(m_pTextBox1->HitTest(pt))
		SwitchFocus(m_pTextBox1);
	else if(m_pTextBox2->HitTest(pt))
		SwitchFocus(m_pTextBox2);
	else if(m_pTextBox3->HitTest(pt))
		SwitchFocus(m_pTextBox3);

	CTClientUIBase::OnLButtonDown( nFlags, pt);
}

void CTSecuritySystemDlg::OnNotify(DWORD from, WORD msg, LPVOID param)
{
	CTClientUIBase::OnNotify(from, msg, param);
}

void CTSecuritySystemDlg::OnKeyUp(UINT nChar, int nRepCnt, UINT nFlags)
{
	CTClientGame* pGame = CTClientGame::GetInstance();

	if (!CanProcess())
		return;

	if (nChar == VK_RETURN && m_pMiddleBtn->IsVisible() && m_pMiddleBtn->IsEnable())
		pGame->OnGM_SC_MIDDLEBTN();
	else
		CTClientUIBase::OnKeyUp(nChar, nRepCnt, nFlags);
}