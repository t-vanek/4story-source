#pragma once


class CTClientPet : public CTClientRecall
{
protected:
	WORD		m_wPetID;
	CTime		m_tPetEndTime;
	LPTPET		m_pPetTemp;
	CString		m_strPetName;
	DWORD			m_dwTakeUpPivot;
	CTClientChar*	m_pTakeUpChar;

	FLOAT		m_fBaseSpeedFactor;
	FLOAT		m_fBaseSpeedFactor_org;
	BYTE        m_bPetEffect;

public:
	void SetPetInfo(const CString& strName, WORD wPetID, const CTime& tEndTime, BYTE m_bEffect);
	void SetEffect(BYTE m_bEffect);
	void SetSaddle(WORD m_wSaddle);
	BOOL TakeUp(
		CD3DDevice *pDevice,
		CTachyonRes *pRES,
		CTClientChar *pChar,
		DWORD dwPivot);

	CTClientChar* TakeDown();
	FLOAT GetSpeedWhenRiding();
	void SetSpeedWhenRiding(WORD wMulti);

	CTClientChar* GetTakeUpChar() const		{ return m_pTakeUpChar; }
	const CString& GetPetName() const		{ return m_strPetName; }
	WORD GetPetID() const					{ return m_wPetID; }
	const CTime& GetPetEndTime() const		{ return m_tPetEndTime; }
	LPTPET GetPetTemp() const				{ return m_pPetTemp; }
	const CString& GetPetKindName() const	{ return m_pTEMP->m_strNAME; }
	BYTE GetPetEffect() const               { return m_bPetEffect; }
	virtual BYTE IsDrawNameWhenDead() { return FALSE; }

public:
	virtual BYTE GetDrawName();
	virtual CString GetTitle();
	virtual CString GetName();
	virtual CString GetUserTitle();
	virtual void ShowSFX();
	virtual void HideSFX();

	virtual BYTE CheckFall(
		CTClientMAP *pMAP,
		LPD3DXVECTOR2 pFallDIR);

	virtual void CalcFrame(BOOL bUpdate);
	virtual void CalcHeight(LPD3DXVECTOR3 pPREV, CTClientMAP *pMAP, DWORD dwTick);
	virtual void Render(CD3DDevice *pDevice, CD3DCamera *pCamera);
	void ApplyPEffect(CTClientRecall* pRecall, BYTE m_bEffect);
	LPTITEMGRADEVISUAL GetPETVISUAL(BYTE m_bEffect);

public:
	CTClientPet();
	virtual ~CTClientPet();
};
