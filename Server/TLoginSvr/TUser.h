/*
 *	TUser.h
 */
#pragma once

#include "stdafx.h"

class CTUser : public CTLoginSession
{
public:
	CTUser();
	virtual ~CTUser();

	virtual BOOL Say( CPacket *pPacket);

public:
	CString Zombie3;
	CString m_strUserID;
	CString Zombie1;
	CString Zombie2;
	CString m_strPasswd;

	DWORD	m_dwID;
	BYTE	m_bGroupID;

	BYTE	m_bLogout;
	BYTE m_bCreateCnt;
	BYTE m_bAgreement;

	DWORD m_dwSendTick;
	INT64 m_dlCheckKey;
	QTDWORD m_qCheckPoint;
	CString m_strCode;
	CString m_strMacAddress;
	CString m_strMail;

	BYTE m_bTries;
	DWORD m_dwTimeLeft;
	BYTE m_bLock;

	void SendEmailWithCode(CString strCode, CString strMail);
#ifdef	DEF_UDPLOG
	VTCHAR m_vCHAR;

	LPTCHARACTER FindCharacterBase(DWORD pCharID);

	void InsertCharacter(LPTCHARACTER pChar);
	void DeleteCharacter(DWORD pCharID);
#endif



public:
	// Control Server Message
	void SendCT_SERVICEMONITOR_REQ(DWORD dwTick, DWORD dwSession, DWORD dwUser, DWORD dwActiveUser);

	// CS message sender
	void SendCS_LOGIN_ACK(
		BYTE bResult,
		DWORD dwUserID,
		DWORD dwCharID,
		DWORD dwKEY,
		DWORD dwIPAddr,
		WORD wPort,
		BYTE bCreateCnt,
		BYTE bInPcBang,
		DWORD dwPremium,
		__time64_t dCurTime/*,
		CString strReason = "",

		__int64 iTimeLeft = 0,
		BYTE bEternal = FALSE*/);

	void SendCS_CREATECHAR_ACK(
		BYTE bResult,
		DWORD dwID,
		CString strNAME,
		BYTE bSlotID,
		BYTE bClass,
		BYTE bRace,
		BYTE bCountry,
		BYTE bSex,
		BYTE bHair,
		BYTE bFace,
		BYTE bBody,
		BYTE bPants,
		BYTE bHand,
		BYTE bFoot,
		BYTE bCreateCnt,
		BYTE bLevel);

	void SendCS_DELCHAR_ACK(
		BYTE bResult,
		DWORD dwCharID);

	void SendCS_START_ACK(
		BYTE bResult,
		DWORD dwMapIP,
		WORD wPort,
		BYTE bServerID);

	
	void SendCS_TESTVERSION_ACK(WORD wVersion);			// Çö½Â·æ CS_TESTVERSION_ACK
	void SendCS_HOTSEND_ACK();
	void SendCS_VETERAN_ACK(BYTE bOption, BYTE bFirstLevel, BYTE bSecondLevel, BYTE bThirdLevel);
	void SendCS_SECURITYCONFIRM_REQ();
	void SendCS_SECURITYRESULT_ACK(BYTE bResult);
	void SendCS_BOWPLAYERNOTIFY_ACK(BYTE bSlot);

#ifdef	DEF_UDPLOG
			
#endif

};
