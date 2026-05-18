#pragma once

enum OUTRO_TYPE
{
	OUTRO_CHARSEL,
	OUTRO_EXIT,
	OUTRO_COUNT
};

class COutro : public CTClientUIBase
{
public:
	TComponent* m_pImage;
	TComponent* m_pSoulLotteryWarn;
	TComponent* m_pWarn2;
	TComponent* m_pLogoutWait;

	TButton* m_pQuit;
	TButton* m_pBackToGame;
protected:
	CD3DDevice* m_pDevice;
public:
	BYTE m_bType;
protected:
	void FadeBack();
	void GoBack();
	void ConfirmExit();
public:
	void UpdateTick(DWORD dwTick);
public:
	virtual void ShowComponent(BOOL bVisible);
	virtual HRESULT Render(DWORD dwTickCount);
	virtual void OnKeyUp(UINT nChar, int nRepCnt, UINT nFlags);
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
public:
	COutro(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~COutro();
};
