#pragma once

#include "TPetDlg.h"

class CTPetManageDlg : public CTPetDlg
{
public:
	typedef std::vector<CTClientPet*>	ClientPetArray;

protected:
	TScroll*		m_pTSCROLL;
	TButton*		m_pRecallBtn;
	BOOL			m_bRecalling;

	TButton*		m_pCEffectBtn;
	TComponent*		m_pCEffectBtnTpl;

	TButton*		m_pDEffectBtn;
	TComponent*		m_pDEffectBtnTpl;

	TComponent*		m_pLEffect;
	TComponent*		m_pLCash;

	INT				m_nListTop;
	INT				m_nListSelect;
	TComponent*		m_pTNAME[5];
	TComponent*		m_pTSTATE[5];
	TComponent*		m_pTPERIOD[5];
	TImageList*		m_pTICON[5];
	TComponent*		m_pTHOVER[5];
	TComponent*		m_pTSEL[5];

	TComponent* m_pTime;
	TComponent* m_pSaddleName;
	TComponent*		m_pSpeed;

	INT				m_nPrvSelIdx;
	ClientPetArray	m_vPetArray;
	WORD            m_wSaddle;

protected:
	void UpdateScrollPosition();
	void SelectUp(int nLine);
	void SelectDown(int nLine);
	BYTE SetCurSelItem( int nLine);

public:
	void AddPet(const CString& strName, WORD wPetID, const CTime& tEndTime, BYTE m_bEffect);
	void SetPet(INT nIdx, const CString& strName, WORD wPetID, const CTime& tEndTime, BYTE m_bEffect);
	void SetSaddle(WORD m_wSaddle, const CTime& tEndTime, BYTE m_bType);
	
	void UpdatePetInfo(INT nIdx);

	void RemovePet( WORD wPetID );
	void ClearPet();

	INT GetPetCount() const;
	BOOL IsPetEmpty() const;

	BYTE GetSaddleSpeed(WORD m_wSaddleID);

	CTClientPet* GetPet(INT nIdx) const;
	CTClientPet* GetSelectPet() const;
	INT GetSelectIdx() const;
	
	INT FindPetByID(WORD wPetID) const;
	INT FindPetByName(const CString& strName) const;

	BOOL IsPetRecall(CTClientPet* pPet);
	WORD GetPetSaddle() const { return m_wSaddle; }
	void SetRecalling(BOOL bRecalling);
	BOOL IsRecalling() const			{ return m_bRecalling; }

public:
	TImageList*		m_pSaddle;
	virtual void SetDisplayPet(const CString& strName, WORD wPetID, const CTime& tEndTime) {}
	virtual void Update(DWORD dwTickCount);

	CString	GetEffectString(BYTE bEffect);

	virtual void OnNotify( DWORD from, WORD msg, LPVOID param );
	virtual void ShowComponent(BOOL bVisible = TRUE);
	virtual HRESULT Render(DWORD dwTickCount);
	void ResetRace( BYTE bRaceID );
	virtual void OnKeyDown( UINT nChar, int nRepCnt, UINT nFlags );
	virtual BOOL DoMouseWheel( UINT nFlags, short zDelta, CPoint pt);
	virtual void OnLButtonUp( UINT nFlags, CPoint pt );
	virtual BYTE OnBeginDrag(
		LPTDRAG pDRAG,
		CPoint point);
public:
	CTPetManageDlg(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc, CD3DDevice* pDevice);
	virtual ~CTPetManageDlg();
};