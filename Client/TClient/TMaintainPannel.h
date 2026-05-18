#pragma once

bool IsPREMIUMMaintain( CTClientMaintain* pMAINTAIN );
bool IsPREMIUMMaintain( LPTSKILL pTSKILL );


class CTMaintainPannel : public CTClientUIBase
{
public:
	CTMaintainPannel( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTMaintainPannel();

public:
#ifdef NEW_IF
		void ResetMAINTAIN(
		CTClientObjBase *pOBJ,
		DWORD dwTick,
		CTClientObjBase* pMainChar);
#else
		void ResetMAINTAIN(
		CTClientObjBase *pOBJ,
		DWORD dwTick);
#endif


	void ResetROTATION( DWORD dwTick);
	void HideAll();
	void ClickedPNext();
	void ClickedPPrev();
	void ClickedNNext();
	void ClickedNPrev();
	void SetMainOBJ(CTClientObjBase* pOBJ);

public:
	virtual BOOL HitTest( CPoint pt);
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	virtual void OnLButtonDown(UINT nFlags, CPoint pt);
	virtual BOOL DoMouseWheel( UINT nFlags, short zDelta, CPoint pt);

protected:
	vector<TImageList*>			m_pMAINTAIN_ICON[ TMTYPE_COUNT ];
	vector<TComponent*>			m_pMAINTAIN_TIME[ TMTYPE_COUNT ];
	vector<LPTSKILL>			m_vMAINTAININFO_TSKILL[ TMTYPE_COUNT ];
	vector<BYTE>				m_vMAINTAININFO_LEVEL[ TMTYPE_COUNT ];
	TButton*					m_pNextBtn[TMTYPE_COUNT];
	BYTE						m_bCount[TMTYPE_COUNT];
	CTClientObjBase*			m_pOBJ;
#ifdef NEW_IF
	CTClientObjBase*			m_pMainOBJ;
#endif
};
