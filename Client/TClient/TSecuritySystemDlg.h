#pragma once

class CTSecuritySystemDlg : public CTClientUIBase
{

protected :
	// (1) = Erstellen (2) = Ändern (3) = Abfragen
	TComponent*		m_pDlgTitle;			//Dialogtitel = Create Securecode(1)																				ID = 13
	TComponent*		m_pTitle;				//Anweisung(Rot) = Please create a Securecode(1)																	ID = FBB	
	TComponent*     m_pHint;				//Hinweis(Hellblau) = A Securecode must be 8-12 characters in length with a number and a special character.(1)		ID = FDF
	TComponent*     m_pBlueTxt;				//Invalidcodehinweis(Hellbalu) = The entered Securecode is invalid. Please try it again. (3)						ID = FE0
	TComponent*     m_pYellowTxt;			//Invalidcodehinweis(Gelbweiß) = The entered Securecode is invalid. Please try it again. (3)						ID = FE1
	TComponent*     m_pShowText;			//Text für die Checkbox = Do not show again																			ID = 6B35
	
	TComponent*		m_pStrCode1;			// = Old Securecode		 Text																						ID = 6B27
	TComponent*		m_pStrCode2;			// = Enter Securecode	 Text																						ID = 6B28
	TComponent*     m_pStrCode3;			// = Confirm Code		 Text																						ID = 6B29
	TEdit*			m_pTextBox1;			// == Old Securecode	 textbox																					ID = 6B24
	TEdit*			m_pTextBox2;			// == Enter Securecode	 textbox																					ID = 6B25
	TEdit*			m_pTextBox3;			// == Confirm Securecode textbox																					ID = 6B26
	TComponent*		m_pTextBox1BackImg;
	TComponent*		m_pTextBox2BackImg;
	TComponent*		m_pTextBox3BackImg;
	TComponent*		m_pCheckBox;
	
	TComponent*		m_pShowCheckbox;		//Checkbox ob es nocheinmal angezeigt werden soll	

	TButton*		m_pMiddleBtn2;			//Middle Button Numero 2
	TButton*		m_pRightBtn;			//Right Button
	TButton*		m_pLeftBtn;				//Left Button
	

private :
	BYTE			m_bDlgType;				//1 = Create 2 = Change 3 = Disable 4 = Wrongcode 
	BYTE			m_bOpenType;			//1 = Login  2 = Über Menüleiste
	BYTE			m_bSessionType;			//1 = Textbox Editieren 2 = Kein Editieren
	BYTE			m_dwAdviseTick;			//Rendertick für Advisetext
	BYTE			m_bPreDlgType;			//Für Securecode Error
	BYTE			m_bButtonDown;			//1 = Links 2 = Mitte 3 = Rechts
	BYTE			m_bWrongAttempts;		//Wie oft der code falsch eingegeben wurde		

	BOOL			m_bEnabled;
	BOOL			m_bExist;
	BOOL			m_bLocked;
	BYTE			m_bLockHours;
	BYTE			m_bLockMinutes;

	void SetDlgType(BYTE bDlgType);
	void SetOpenType(BYTE bOpenType);

	void DisplayCreateMode();
	void DisplayEnableMode();
	void DisplayDisableMode();
	void DisplayChangeMode();
	void DisplayWrongCode();
	void DisplayUnlockMode();
	void DisplayUnlockedMode();
	void DisplayDisabledMode();
	void DisplayChangedMode();
	void DisplayTempLockedMode();

	BOOL bTitleShow;
	BOOL bHintShow;
	BOOL bBlueTextShow;
	BOOL bYellowTextShow;
	BOOL bstrCode1Show;
	BOOL bstrCode2Show;
	BOOL bstrCode3Show;
	BOOL bMiddleBtnShow;
	BOOL bMiddleBtn2Show;
	BOOL bRightBtnShow;
	BOOL bLeftBtnShow;
	BOOL bTextBox1Show;
	BOOL bTextBox2Show;
	BOOL bTextBox3Show;
	BOOL bCheckBoxShow;
	BOOL bCheckBoxTxtShow;

public :
	TButton*		m_pMiddleBtn;			//Middle Button
public:


	void PrepareDisplay();
	void RenderAdvise();

	BOOL CheckCode(LPCSTR str);

	TEdit* GetTextBoxContent(BYTE bBox = 0);

	void GetTextBoxString(BYTE bID, CString & Output);

	BYTE GetDlgType() const			{ return m_bDlgType; }
	BYTE GetOpenType() const		{ return m_bOpenType; }
	BYTE GetWrongAttempts() const   { return m_bWrongAttempts; }
	BYTE GetPreDlgType() const		{ return m_bPreDlgType; }
	BYTE GetEnabled() const			{ return m_bEnabled; }
	BYTE GetExist() const			{ return m_bExist; }
	BYTE GetLocked() const			{ return m_bLocked; }

	void SetWrongAttempts(BYTE bWrongAttempts);
	void SetEnabled(BOOL bEnabled);
	void SetExist(BOOL bExist);
	void SetLockTime(BYTE bHour, BYTE bMinute);
	void SetLocked(BYTE bLock);

	virtual void OnLButtonDown(UINT nFlags, CPoint pt);

	virtual void OnNotify(DWORD from, WORD msg, LPVOID param);

	virtual void OnKeyUp(UINT nChar, int nRepCnt, UINT nFlags);

	BYTE SwitchMode(BYTE bDlgType, BYTE bOpenType = 0);
	HRESULT Render(DWORD dwTickCount);

	CTSecuritySystemDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTSecuritySystemDlg();	
};
