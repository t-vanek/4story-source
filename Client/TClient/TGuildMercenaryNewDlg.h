#pragma once
class CTGuildMercenaryNewDlg: public ITInnerFrame
{
public:
	static BYTE		m_bTabIndex;
	typedef CTGuildCommander::GuildDetInfo GuildDetInfo;
	typedef CTGuildCommander::GuildMember GuildMember;
	typedef CTGuildCommander::GuildTactics GuildTactics;
	typedef CTGuildCommander::GuildMemberVec GuildMemberVec;
	typedef CTGuildCommander::GuildTacticsVec GuildTacticsVec;
	typedef CTGuildCommander::PopupBtnInfo PopupBtnInfo;

public:
	static const PopupBtnInfo m_PopupBtnInfo[TGM_PBTN_COUNT];

public:
	void ShowPopup(const CPoint& pt);
	void CancelPopup();
	BOOL IsPopupVisible() const;
	//void EnableGuildUI( BOOL bEnable );
	BOOL ShowPopupCastle( WORD wCastleID );

public:
	void RequestInfo();
	void ResetInfo();
	virtual HRESULT Render( DWORD dwTickCount);
	virtual void OnLButtonDown( UINT nFlags, CPoint pt );
	virtual void OnLButtonUp( UINT nFlags, CPoint pt );

public:
	int OnGM_GMP_WHISPER2();		
	int OnGM_GMP_MAIL();
	int OnGM_GMP_PARTY();
	int OnGM_GMP_FRIEND();
	int OnGM_GMP_DISORG_TACTICS();
	int OnGM_GMP_CANCEL();
	int OnGM_GMP_VOLUNTEER_HESED();
	int OnGM_GMP_VOLUNTEER_ARDRI();
	int OnGM_GMP_VOLUNTEER_TYCON();
	int OnGM_GMP_VOLUNTEER_GEHBRA();
	int OnGM_GMP_VOLUNTEER_CANCEL();
	int OnGM_GMP_TACTICS_RE_CONTRACT();
	int OnGM_GMP_TACTICS_KICK();
	int OnGM_GMP_TACTICS_SELF_KICK();
	int OnGM_GMP_TACTICS_REWARD_POINT();

public:
	TList* m_pListC;
	BOOL m_bPopupVisible;
	CTMiniPopupDlg* m_pPopup;
	INT m_nLastSelType;
	INT m_nLastSelIdx;
	TListItem* m_pDownItem;


public :
	CTGuildMercenaryNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, FRAMEDESC_SHAREDPTR pPopupDesc);
	virtual ~CTGuildMercenaryNewDlg();
};