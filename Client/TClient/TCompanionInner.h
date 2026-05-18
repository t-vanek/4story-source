#pragma once

#include "TClientCompanion.h"

typedef std::map<BYTE, CTClientCompanion*> Spolecnici;
class CTCompanionDlg : public CTFrameGroupBase
{
public:
	TImageList*	m_pPet[5];
	TImageList* m_pIcon;
	TComponent* m_pName;
	TComponent* m_pBonusName;
	TComponent* m_pStatus;
	TButton* m_pSummon;
	TButton* m_pDelete;
	TButton* m_pLevelUp;
	TButton* m_pChangeEffect;
	TComponent* m_pHP;
	TComponent* m_pExp;
	TGauge* m_pHPBar;
	TGauge* m_pExpBar;
	TComponent* m_pBonusT;
	TComponent* m_pArrowUP;
	TComponent* m_pTab1Shown;

	TComponent* m_pStatPText;
	TComponent* m_pStatPValue;
	TImageList* m_pSlot1T;
	TImageList* m_pSlot2T;
	TButton* m_pLeftSwitch;
	TButton* m_pRightSwitch;
	TComponent* m_pLevel;

	TGauge* m_pItemTicks[2];

	DWORD dwSelExp;
	DWORD dwSelNextExp;
	DWORD dwSelLife;

	BYTE m_bCurSelSlot;
	BYTE m_bSummonedSlot;
	DWORD m_dwSummonedID;
	CTCtrlList*		m_pList;
	BOOL m_bIsSummoned;
	BYTE m_bTab;
	DWORD m_dwTick;

	Spolecnici m_mapSpolecnici;
	BOOL IsCompanionEmpty();

	virtual void ShowComponent( BOOL bVisible );
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	HRESULT Render(DWORD dwTickCount);
	void SelRight();
	void SelLeft();
	void Release();
	void SetSummonedSlot(BYTE bSlot);

	void GetAttrString(CString& strAttr, CTClientCompanion* pCompanion);
	DWORD GetCompanionMonID(BYTE m_bSlot);

	BYTE OnBeginDrag( LPTDRAG pDRAG, CPoint point);
	TDROPINFO OnDrop(CPoint point);
	CTClientCompanion* GetSelectedCompanion( BYTE m_bSlot );
	void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual BOOL CanWithItemUI();

public :
	CTCompanionDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual	~CTCompanionDlg();
};