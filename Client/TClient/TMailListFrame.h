#pragma once
class CTMailSlot;
class CTMailItem;
class CTMailListFrame :  public CTClientUIBase
{
public:
	enum { MAX_LINE = 6 };

	
	
	
	TComponent* m_pMailDlgTitle;
	WORD m_wTotalCount;

		CTCtrlList*		m_pList;
		TButton*	m_pCheckAll;

protected:
	TScroll*	m_pScroll;
	TButton*	m_pSendBtn;
	

	INT			m_nSelectIdx;
	INT			m_nPrvScrPos;
	BOOL		m_bNeedUpdate;
	//Item		m_vItems[MAX_LINE];
	CTClientUIBase* m_pFRAME;



public:
	void AddMail(LPTMAIL_SIMPLE pMail);
	void SetMail(INT nIdx, LPTMAIL pMail);
	void SortMail();

	void RemoveMail(INT nIdx);
	void ClearMail();
	
	LPTMAIL GetMail(INT nIdx) const;
	LPTMAIL_SIMPLE GetMailSimple(INT nIdx) const;
	
	void SetSelectedIndex(INT nIdx);
	INT GetSelectedIndex() const;
	INT FindIndexByPostID(DWORD dwPostID) const;

	BOOL IsEmpty() const;
	UINT GetCount() const;

	void ViewMail(INT nIdx);
	BOOL IsNewMail() const;

	void NotifyUpdate();
	void Update();
	void SetInfo(WORD wTotalCount, WORD wNotReadCount, WORD wPage);

public:
	virtual void RequestInfo()	{}
	virtual void ResetInfo()	{}

	virtual BOOL DoMouseWheel( UINT nFlags, short zDelta, CPoint pt);
	virtual void ShowComponent( BOOL bVisible = TRUE);
	virtual HRESULT Render( DWORD dwTickCount );
	virtual void OnLButtonDown(UINT nFlags, CPoint pt);	
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual void OnRButtonDown(UINT nFlags, CPoint pt);

public:
	CTMailListFrame(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTMailListFrame();
};

class CTMailSlot : public CTCtrlListSlot
{
public:
		BOOL			m_bShow;

		TComponent*		m_pMailImg;
		TComponent*		m_pPackage;

		TComponent*		m_pSenderTxt;
		TComponent*		m_pTitleTxt;
		TComponent*		m_pTimeTxt;

		TButton*		m_pCheck;
		TButton*		m_pButton;

		LPTMAIL_SIMPLE	m_pMail;

public:
	virtual void ShowComponent(BOOL bShow);
		//void SetMail(LPTMAIL_SIMPLE pMail);
	virtual void Select(BOOL bSel);

public:
	CTMailSlot(): m_pMail(NULL),m_pMailImg(NULL){}
		virtual ~CTMailSlot() {}
};

class CTMailItem : public CTCtrlListItem
{
	public:
		LPTMAIL_SIMPLE	pSimple;
		LPTMAIL			pInfo;
		BYTE			SelectToDel;
	public:
		virtual void ReflectSlot(CTCtrlListSlot* pSlot);
	public:
		CTMailItem(): pSimple(NULL),pInfo(NULL),SelectToDel(FALSE){}
			virtual ~CTMailItem() {}
};