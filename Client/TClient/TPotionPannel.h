#pragma once

bool IsPREMIUMMaintain( CTClientMaintain* pMAINTAIN );
bool IsPREMIUMMaintain( LPTSKILL pTSKILL );

class CTPotionPannel : public CTClientUIBase
{
public:
	CTPotionPannel( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTPotionPannel();

public:
	void ResetPOTIONS(
		CTClientObjBase *pOBJ,
		DWORD dwTick);

	void AddMaintain(BYTE bIndex, BYTE bIcon, DWORD dwTick, CTClientMaintain* pSkill, CTClientObjBase* pChar);
	void FixMaintain(BYTE bIndex, CTClientMaintain* pSkill, CTClientObjBase* pChar);
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	void SetNewChar(CTClientObjBase* pMainChar);
	void ResetROTATION( DWORD dwTick);
	HRESULT Render(DWORD dwTickCount);
	virtual BOOL HitTest( CPoint pt);
	void ClickedPNext();
	void ClickedPPrev();
	void ClickedNNext();
	void ClickedNPrev();
	BYTE	m_bIsHidden;	
	BYTE	m_bCount;
	void HideAll();
	
protected:
	TImageList*					m_pMAINTAIN_ICON[ 10 ];
	TComponent*					m_pMAINTAIN_TIME[ 10 ];
	LPTSKILL					m_vMAINTAININFO_TSKILL[ 10 ];
	BYTE						m_vMAINTAININFO_LEVEL[ 10 ];
	
	CTClientObjBase*			m_pOBJ;
	CTClientObjBase*			m_pMainChar;
};
