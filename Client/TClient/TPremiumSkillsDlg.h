#pragma once
#define  PREMIUM_SKILLS_ONPAGE       6

class CTPremiumSkills : public CTClientUIBase
{
public:
	TComponent* m_pMedals  [PREMIUM_SKILLS_ONPAGE];
	TComponent* m_pHotkey  [PREMIUM_SKILLS_ONPAGE];
	TGauge*     m_pTick    [PREMIUM_SKILLS_ONPAGE];
	TImageList* m_pPressed [PREMIUM_SKILLS_ONPAGE];
	TImageList* m_pIcon    [PREMIUM_SKILLS_ONPAGE];

public:
	CTClientChar* m_pHost;
	BYTE m_bType;

public:
	MAPPREMIUMSKILL m_mapPREMIUMSKILL;

public:
	void AddSkill(BYTE bID, WORD wSkillID);
	void Init();
	void ResetHotkeyStr();
	void CalcHotkeyTick();
	void PushIcon( CPoint point );
	void ReleaseIcon();
	WORD FindSkillID(BYTE bID);
	void Clear();

public:
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	CTPremiumSkills( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, CTClientChar* pHost, BYTE bType );
	virtual ~CTPremiumSkills();
	virtual BOOL	CanWithItemUI();
	virtual void OnLButtonDown( UINT nFlags, CPoint pt);
	virtual void OnLButtonUp( UINT nFlags, CPoint pt);
	virtual void OnMouseMove( UINT nFlags, CPoint pt);
	virtual void ResetPosition();
	virtual BYTE OnBeginDrag(LPTDRAG pDRAG, CPoint point);
	virtual TDROPINFO OnDrop(CPoint point);
};