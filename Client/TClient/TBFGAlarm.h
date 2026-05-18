#pragma once


class CTBFGAlarm : public CTClientUIBase
{
public:
	enum BR_TYPE
	{
		BR_SOLO = 0,
		BR_TEAM,
	};

private:
	BYTE m_bType;
	BYTE m_bBRType;
	DWORD m_dwDefaultTextColor;
	BYTE m_bMovingForward;
	BYTE m_bMovingBackward;
public:
	TButton*	m_pAlarm;
	TComponent* m_pText;
	TComponent* m_pTime;
public:
	BYTE GetType() { return m_bType; }
	BYTE GetBRType() { return m_bBRType; }

	void SetTime(DWORD dwSecond);

	void PopUp();

	void SetType(BYTE bType, BYTE bBRType = BR_SOLO);
	virtual HRESULT Render(DWORD dwTickCount);
	virtual void OnMouseMove(UINT nFlags, CPoint pt);
	void SetTextColor(BYTE bStatus, BYTE bBRType = BR_SOLO);
	virtual void ResetPosition();
	virtual void ShowComponent(BOOL Visible);
public:
	CTBFGAlarm( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBFGAlarm();
	virtual BOOL	CanWithItemUI();
};