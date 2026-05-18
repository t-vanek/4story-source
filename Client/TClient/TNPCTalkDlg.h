#pragma once

class CTNPCTalkDlg : public CQuestNewDlg
{
public:
	CQuestNewDlg* m_pTFollowDlg;
	VTQUESTTEMP m_vTDEFTALK;

	CQuest m_BQuest;
public:
	CQuest* GetCurSelTQuest();
	LPTQUEST GetCurTMission();
	LPTQUEST GetCurTQuest();

	void Talk( CString strTALK);
	void Talk( CString strTALK, CString strAnswer);
	void DefaultTalk();

public:
	virtual void ShowComponent( BOOL bVisible = TRUE);

	virtual ITDetailInfoPtr GetTInfoKey( const CPoint& point);
	virtual HRESULT Render( DWORD dwTickCount);

public:
	CTNPCTalkDlg( TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc, CTClientChar* pHost, TCMLParser* pParser);
	virtual ~CTNPCTalkDlg();
};
