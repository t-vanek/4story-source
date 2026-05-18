#pragma once
class CTGuildSkillNewDlg: public ITInnerFrame
{
public:
	static BYTE		m_bTabIndex;

public:
	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point );

public:
	virtual void RequestInfo()	{}
	virtual void ResetInfo()	{}

public:
	virtual HRESULT Render( DWORD dwTickCount);

public :
	CTGuildSkillNewDlg( TComponent *pParent, LP_FRAMEDESC pDesc);
	virtual ~CTGuildSkillNewDlg();
};