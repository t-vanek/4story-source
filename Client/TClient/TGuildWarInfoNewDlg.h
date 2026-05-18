#pragma once
class CTGuildWarInfoNewDlg: public ITInnerFrame
{
public:
	static BYTE		m_bTabIndex;

	enum 
	{
		MODE_CASTLE=0,
		MODE_LOCAL,
		MODE_MISSION,
		MODE_COUNT
	};
	typedef CTGuildCommander::GuildDetInfo			GuildDetInfo;
	typedef CTGuildCommander::Territory				Territory;
	typedef CTGuildCommander::LocalTerritory		LocalTerritory;

protected:
	TList*		m_pList;
	TList*		m_pList2;
	TList*		m_pList3;

public:
	void SetCurMode();

public:
	virtual void RequestInfo();
	virtual void ResetInfo();

public:
	virtual void OnLButtonDown( UINT nFlags, CPoint pt );
	virtual void SwitchFocus(TComponent *pCandidate);
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );
	virtual void ShowComponent(BOOL bVisible = 1);

public :
	CTGuildWarInfoNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTGuildWarInfoNewDlg();
};