#pragma once


class CTClientSpolecnik : public CTClientMoveObj
{
public:
	static D3DXVECTOR2 m_vTPOS[TRECALL_MAX];

public:

	LPTMONTEMP m_pTEMP;


	DWORD m_dwTargetID;
	DWORD m_dwHostID;

	

	BYTE m_bTargetType;
	BYTE m_bSpolecnikType;
	BYTE m_bSubAI;
	BYTE m_bTPOS;
	BYTE m_bDEAD;
	BYTE m_bDIE;
	BYTE m_bAI;
	

    WORD		m_wMonID;
	DWORD       m_dwHp;
	DWORD		m_dwExp;
	BYTE		m_bLevel;
	BYTE		m_bSkillPoint;	
	BYTE		bSlot;

	BYTE		m_bSTR;
	BYTE		m_bDEX;
	BYTE		m_bCON;
	BYTE		m_bINT;
	BYTE		m_bWIS;
	BYTE		m_bMEN;
	BYTE		m_bBonus1;
		
	CString		m_strSpolecnikName;


	WORD m_wItemID;
	DWORD m_dwRemainTick;
	__int64 m_ldwEndTime;


	CTClientItem* m_pItem;
	CTClientItem* m_pCostume;
	

	INT m_nSpolecnikRunAwayIndex;
	TRUNAWAY_ARRAY m_vSpolecnikRunAway;
	D3DXVECTOR3 m_vSpolecnikRunAwayTarget;

	BOOL m_bTrans;


	BOOL m_bWalking;

	BYTE m_bEffect;
	BYTE m_bCanTeleport;

public:
	FLOAT GetLOST( CTClientObjBase *pTARGET);
	FLOAT GetAB( CTClientObjBase *pTARGET);
	FLOAT GetLB( CTClientObjBase *pTARGET);
	BYTE  GetSlot();

	
	BYTE GetRoamACT( LPD3DXVECTOR3 pTARGET);

public:
	virtual DWORD CalcJumpDamage();
	virtual DWORD CalcFallDamage();
	virtual BYTE IsAlliance( CTClientObjBase *pTARGET);
	virtual BYTE Fall( LPD3DXVECTOR2 pFallDIR);
	virtual BYTE GetDrawName();
	virtual void ReleaseData();
	virtual CString GetTitle();
	virtual CString GetName();
	virtual DWORD GetHostID();
	virtual BYTE GetTAction();
	virtual BYTE CanDIVE();
	virtual void InitSpolecnik(
		CD3DDevice *pDevice,
		CTachyonRes *pRES,
		WORD wTempID,
		BYTE bLevel);

	virtual D3DXVECTOR3 GetRoamTarget(
		LPD3DXMATRIX pDIR,
		FLOAT fPosX,
		FLOAT fPosY,
		FLOAT fPosZ);

	virtual D3DXVECTOR3 AdjustRoamTarget(
		CTClientChar* pHOST,
		CTClientMAP* pMAP,
		LPD3DXMATRIX pDIR,
		D3DXVECTOR3 vTARGET );

	virtual BYTE CheckFall(
		CTClientMAP *pMAP,
		LPD3DXVECTOR2 pFallDIR);

	virtual LRESULT OnActEndMsg();
	virtual void CalcHeight( LPD3DXVECTOR3 pPREV, CTClientMAP *pMAP, DWORD dwTick);
	virtual void DoTRANS( CD3DDevice *pDevice, CTachyonRes *pRES, WORD wMonID);
	virtual void DoRETRANS( CD3DDevice *pDevice, CTachyonRes *pRES);


	virtual void SetTAction(BYTE bAction);
	virtual void SetEffect(BYTE bEffect);

	virtual void DrawEffect();
	
	virtual void Render(CD3DDevice *pDevice, CD3DCamera *pCamera);

	
	LPTITEMGRADEVISUAL GetPETVISUAL(BYTE m_bEffect);
	BYTE GetPetBallID();

public:
	CTClientSpolecnik();
	virtual ~CTClientSpolecnik();
};
