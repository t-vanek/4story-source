#pragma once

class CTItemRegBox : public CTClientUIBase
{
public:
	enum { MAX_COUNT_POSCNT = 3 };
	enum PRIVATESHOP_SELL_CURRENCY 
	{
		CURRENCY_MONEY = 0,
		CURRENCY_CREDIT,
		CURRENCY_COUNT
	};
public:
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
protected:
	TComponent*		m_pTitle;
	TButton*		m_pOK;
	TButton*        m_pCheckBox[CURRENCY_COUNT];

	TEdit*			m_pCount;
	TEdit*			m_pRune;
	TEdit*			m_pLuna;
	TEdit*			m_pCron;
	TEdit*          m_pCredits;

	CTClientItem*	m_pItem;
	BYTE			m_bInven;
	BYTE			m_bInvenSlot;
	INT             m_nCurrencyIndex;

public:
	void SetItem(CTClientItem* pItem, BYTE bInven, BYTE bInvenSlot);
	
	CTClientItem* GetItem() const	{ return m_pItem; }
	BYTE GetInven() const			{ return m_bInven; }
	BYTE GetInvenSlot() const		{ return m_bInvenSlot; }
	
	void SetCount(DWORD dwCnt);
	void SetRune(DWORD dwRune); 
	void SetLuna(DWORD dwLuna); 
	void SetCron(DWORD dwCron); 
	void SetOkGM( DWORD dwGM );

	DWORD GetCount() const;
	DWORD GetRune() const;
	DWORD GetLuna() const;
	DWORD GetCron() const;
	DWORD GetCredits() const;
	INT GetCurrencyIndex() const;

	TEdit* GetCurEdit();

	virtual BOOL CanWithItemUI();
	virtual HRESULT Render(DWORD dwTickCount);

public:
	CTItemRegBox( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTItemRegBox();
};