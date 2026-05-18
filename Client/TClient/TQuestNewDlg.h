#pragma once

#define BOX_PH_COUNT	2
#define STATIC_MAX		24

class CRightSide
{
public:
	CTClientUIBase* m_pFrame;

	TComponent* m_pRewardDot;
	TComponent* m_pRewardText;

	TComponent* m_pItemFrame[STATIC_MAX];

	TImageList* m_pSkillIcon[STATIC_MAX];
	TImageList* m_pItemIcon[STATIC_MAX];
	TImageList* m_pTitleIcon[STATIC_MAX];

	TComponent* m_pRewardName[STATIC_MAX];

	TComponent* m_pExpBox[BOX_PH_COUNT];
	TComponent* m_pExpVal;

	TComponent* m_pSoulBox[BOX_PH_COUNT];
	TComponent* m_pSoulVal;

	TComponent* m_pAVBox[BOX_PH_COUNT];
	TComponent* m_pAVVal;

	TComponent* m_pMoneyBox[BOX_PH_COUNT];
	TComponent* m_pMoney[3];

	TComponent* m_pTermDot;
	TComponent* m_pTermText;

	TComponent* m_pTermStr[STATIC_MAX];
	TComponent* m_pTermRes[STATIC_MAX];

	TComponent* m_pSummaryDot;
	TComponent* m_pSummaryText;
	TComponent* m_pSummaryStr[STATIC_MAX];

	TComponent* m_pDialogueDot;
	TComponent* m_pDialogueText;
	TComponent* m_pDialogueStr[STATIC_MAX];
public:
	BYTE m_RewardCnt;
	BYTE m_TermCnt;
	BYTE m_SummaryLnCnt;
	BYTE m_DialogueLnCnt;

	BOOL m_Reward;
	BOOL m_Goal;
	BOOL m_Summary;
	BOOL m_Dialogue;

	DWORD m_DefaultClr;

	INT m_Index;
	CPoint m_BasePt;
public:
	void LoadComponent();
	void Reset();
	void ResetPos();
	void AddReward(BYTE Type, DWORD Value, BYTE Count = 0, const LPTREWARD Reward = nullptr, DWORD dwQuestID = 0);
	void ReAlign(INT ScrollY);
	void AddTerm(const LPTTERM& Term, BYTE bCount);
	void SummaryMsg(const CString& strSummary);
	void DialogueMsg(CString& strTitle, const CString& strDialogue);
};

class CQuest
{
public:
	INT m_ID;
	BOOL m_bSelected;
	BOOL m_bInit;
	BOOL m_Accepted;
	BOOL m_Completed;

	TButton* m_pCheckBox;
	TButton* m_pQuestBtn;
	TComponent* m_pQuestName;
	TImageList* m_pProgress;
	TComponent* m_pRegion;

	CTClientUIBase* m_pFrame;
	DWORD_PTR m_dwQuest;

public:
	void DelQuest();
	void AddQuest(CString strQName, CString strLevel);
};

class CQuestList
{
public:
	INT m_ID;
	BOOL m_Opened;

	TButton* m_pQuestCat;
	TComponent* m_pQuestCatReg[255];

	CTClientUIBase* m_pFrame;

	BYTE m_bCatSize;
	BOOL m_bInit;

	std::map<DWORD, std::vector<CQuest>> m_mapQuest;

public:
	void DeleteQuestList();
	void LoadComponent();
	void AddCategory(INT nID, CString strName, BYTE bCatCount);
	void AddQuest(DWORD dwCategoryID, CString strQuestName, CString strLevel, DWORD_PTR dwQuest, BOOL Accepted, BOOL Complete);
	void MoveCatBy(INT nY);
public:
	DWORD GetTQUESTColor(const CQuest & pQUEST) const;
public:
	BYTE GetSize()	{ return (BYTE) m_mapQuest.size(); }
	void Open() { m_Opened = TRUE; }
	void Close() { m_Opened = FALSE; }
	void ReInit() //Call before opening
	{
		m_bInit = FALSE;
	}
};

class CQuestNewDlg : public CTClientUIBase
{
public:
	static CPoint m_ptPOS;

	INT m_nCurLeftScrollY;
	INT m_nDesiredLeftScrollY;

	INT m_nCurRightScrollY;
	INT m_nDesiredRightScrollY;

	CQuestNewDlg(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc, CTClientChar* pHost);
	virtual ~CQuestNewDlg(void);

	virtual HRESULT Render(DWORD dwTickCount);
	virtual void OnLButtonDown(UINT nFlags, CPoint pt);
	virtual void OnLButtonUp(UINT nFlags, CPoint pt);
	virtual void OnRButtonDown(UINT nFlags, CPoint pt);
	virtual void OnRButtonUp(UINT nFlags, CPoint pt);
	virtual void ShowComponent(BOOL bVisible = TRUE);
	virtual ITDetailInfoPtr GetTInfoKey(const CPoint& point);
	virtual BOOL GetTChatInfo(const CPoint& point, TCHATINFO& outInfo);
	virtual BOOL DoMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	virtual void OnMouseMove(UINT nFlags, CPoint pt);
	virtual void MoveComponent(CPoint pt);

	// 퀘스트출력
	void Reset();
	void ResetTQUEST(LPTQUEST pTQUEST, CQuest* Quest);
	void SummaryMessage(const CString& strText);
	void TermMessage(DWORD dwID, const CString& strTERM, const CString& strRESULT, INT nLine, INT& nNumber);
	void TextMessage(CString strTitle, CString strText);
	void TextMessage(DWORD dwTitleID, CString strText);

	CString GetResultString(CTClientQuest* pTQUEST);
	CString GetSpeakerString(CString strSpeaker);
	static DWORD m_DefaultQColor;

	// 외부에서 접근
public:
	CTClientObjBase *m_pHost;
	DWORD m_dwCompleteID;
	DWORD m_dwAcceptID;
	LPTQUEST m_pTQUEST;
	CQuest* m_Quest;

	BYTE m_bPrintMSG;
	LPTREWARD m_pTSELREWARD;
	TButton* m_pAccept;
	TButton* m_pRefuse;
	TList* m_pContents;
	TImageList* m_pStatusIcon[5];

	CString m_strNPCTitle;
	CString m_strNPCTalk;
	CString m_strAnswerWhenNPCTalk;

	TButton* m_pCheckBox;
	TButton* m_pQuestBtn;
	TComponent* m_pQuestName;
	TImageList* m_pProgress;
	TComponent* m_pQuestCatReg;
	TButton* m_pQuestCat;
	TScroll* m_pLScroll;
	TScroll* m_pRScroll;

	TComponent* m_pQuestText;

	TComponent* m_pShowMap;
	TComponent* m_pMapBorder;
	TImageList* m_pMyCharPos[2];
	TImageList* m_pMapList;
	TImageList* m_pNoTerm;
	TComponent* m_pShowWhenMap[4] = { FindKid(26169), FindKid(26170), FindKid(26171), FindKid(26172) };

	static TComponent* m_pRewardDot;
	static TComponent* m_pRewardText;
	// 공통
protected:
	LPTREWARD 		m_pTTOPREWARD;

	// 퀘스트출력
protected:
	TImageList** m_pRewardSkill;
	TImageList** m_pRewardItem;
	TImageList* m_pRewardSel;

	TComponent* m_pTitleReward;

	TComponent* m_pSummaryTxt;
	TComponent* m_pRewardTxt;

	TImageList* m_pITEM[TREWARDITEMCOUNT];
	TImageList* m_pSKILL[TREWARDITEMCOUNT];
	TComponent* m_pSEL[TREWARDITEMCOUNT];
	TComponent* m_pMONEY[3];
	TComponent* m_pEXP;
	TButton *m_pREFUSE;

public:
	TButton *m_pACCEPT;

	// Quest Map
public:
	CTClientCAM *m_pCAM;
	CTClientMAP *m_pTMAP;
	CD3DDevice *m_pDevice;

	INT m_nMainUnitX;
	INT m_nMainUnitZ;

	D3DXVECTOR3 m_ptMapIcon;
	D3DXVECTOR2 m_vTCENTER;
	WORD m_wMapID;

	FLOAT m_fTSCALE;
	BYTE m_bHideMap;
	BYTE m_bRenderMap;

	TButton* m_pMapOpen;
	TButton* m_pMapClose;
public:
	void	GetUnitPosition(FLOAT fPosX, FLOAT fPosY, INT& outUnitX, INT& outUnitY);

	void	ResetMap(LPTQUEST pTQUEST);
	void	RederMapPos(DWORD dwTickCount);

	DWORD	MakeUnitID(WORD wMapID, BYTE nUnitX, BYTE nUnitY);
	CPoint  GetPosition(FLOAT fPosX, FLOAT fPosZ, int nWidth, int nHeight);

	virtual void OnNotify(DWORD from, WORD msg, LPVOID param);
public:
	enum
	{
		COL_ITEM,
		COL_LEVEL,
		COL_STATE,
		COL_CATEGORY,
		COL_COUNT
	};

	void ResetTree(BYTE FindNew);
	void SetQuestState(INT nIdx, LPTQUEST pTQUEST);
	void CheckShowRight(const CQuest& Quest);

	CQuest* GetTQUEST(INT nIdx);
public:
	CQuestNewDlg *m_pQuestDlg;

	CQuestList m_QuestList[3];
	CRightSide m_RightSide;
}; 