#pragma once
typedef enum BATTLE_MODE
{
	MODE_BOW = 0,
	MODE_BR
};
class CTBFGStatus : public CTClientUIBase
{
public:
	TButton*	m_pRank;
	TButton*	m_pSurr;
   
	TComponent* m_pDefugel;
    TComponent* m_pCraxion;

	TComponent* m_pNation1;
	TComponent* m_pNation2;

	TComponent* m_pHeart;
	TComponent* m_pSkull;

	TImageList* m_p1Min;
    TImageList* m_p2Min;
	TImageList* m_p1Sec;
    TImageList* m_p2Sec;

	TComponent* m_pMiss;
    TComponent* m_pYes;
	
	TComponent* m_pNo;
    TComponent* m_pFinish;
public:
	BYTE m_bMode;
public:
	virtual BOOL	CanWithItemUI();
	virtual void OnLButtonUp( UINT nFlags, CPoint pt);
	virtual HRESULT Render(DWORD dwTickCount);
	virtual void ShowComponent(BOOL bVisible = TRUE);
public:
	void UpdateTopUI( BYTE bLifes, WORD wKills, WORD wAssists );
	void SetMode( BYTE bMode );
	void SetTime(BYTE bMin, BYTE bSec);
	void SetPoints(BYTE bDefugelPoints, BYTE bCraxionPoints);

public:
	CTBFGStatus( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBFGStatus();
};