/*
 *	TUser.cpp
 */
#include "StdAfx.h"
#include "jwsmtp\jwsmtp.h"

CTUser::CTUser()
{
	m_strUserID.Empty();
	m_strPasswd.Empty();

	m_bLogout	= FALSE;
	m_dwID		= 0;
	m_bCreateCnt = 0;
	m_bAgreement = TRUE;
	m_strCode.Empty();
	m_strMail.Empty();
	m_bTries = 0;
	m_dwTimeLeft = 0;
	m_bGroupID = 0;

	m_dwAcceptTick = 0;
	m_dwSendTick = 0;
	m_dlCheckKey = 0;

	m_bLock = TRUE;
	
	while(!m_qCheckPoint.empty())
		m_qCheckPoint.pop();

#ifdef DEF_UDPLOG
    m_vCHAR.clear();
#endif
}

CTUser::~CTUser()
{
#ifdef DEF_UDPLOG

	//	Delete the Char List Memory
	for(DWORD i=0; i < m_vCHAR.size(); i++)
		delete m_vCHAR[i];

	m_vCHAR.clear();

#endif
}

BOOL CTUser::Say( CPacket *pPacket)
{
	m_dwSendTick = GetTickCount();
	return CSession::Say(pPacket);
}

/*
 *
 */
#ifdef DEF_UDPLOG


LPTCHARACTER CTUser::FindCharacterBase(DWORD pCharID)
{
	for(DWORD i=0; i < m_vCHAR.size(); i++)
	{
		if( m_vCHAR[i]->m_dwCharID  == pCharID )
			return m_vCHAR[i];
	}

	return NULL;
}


void CTUser::InsertCharacter(LPTCHARACTER pChar)
{
	m_vCHAR.push_back(pChar);

}

void CTUser::DeleteCharacter(DWORD pCharID)
{
	LPTCHARACTER pChar = FindCharacterBase(pCharID);

	if(pChar != NULL)
	{
		vector<LPTCHARACTER>::iterator where = find(m_vCHAR.begin(), m_vCHAR.end(), pChar);
        m_vCHAR.erase( where);	
	}
	
}
#endif
void CTUser::SendEmailWithCode(CString strCode, CString strMail)
{
	CString strMessage;
	strMessage.Format("Hello,\nsomeone unfimiliar has tried to connect to your Account. For security reasons we have refused his attempt to connect to your Account.\nIf this connection attempt was performed by you, please enter the following code in order to connect to your account. \n Code : %s \nAfter entering the code, the person will be white listed for your account and will always be able to connect to it (in case he has your password).\nHowever, we still advise you to use the security code system ingame to save your properties.\n \nIf this login attempt was not caused by you, please contact an administrator and immediately change your account information. ", strCode);

	jwsmtp::mailer mail(strMail, "mail", "4Story 2-Step verification", strMessage,
		"host", 0, false);

	mail.username("");
	mail.password("");

	mail.send();
}
/*
void CTUser::SendEmailWithCode(CString strCode, CString strMail)
{
	CString strMessage;
	strMessage.Format("Hello,\nsomeone unfimiliar has tried to connect to your Account. For security reasons we have refused his attempt to connect to your Account.\nIf this connection attempt was performed by you, please enter the following code in order to connect to your account. \n Code : %s \nAfter entering the code, the person will be white listed for your account and will always be able to connect to it (in case he has your password).\nHowever, we still advise you to use the security code system ingame to save your properties.\n \nIf this login attempt was not caused by you, please contact an administrator and immediately change your account information. ", strCode);

	jwsmtp::mailer mail(strMail, "noreply@games.com", "4Story 2-Step verification", strMessage,
		"mail.mdhostmx.eu", 587, false);

	mail.username("noreply@games.com");
	mail.password(")j)=!!kwsKSYYyoOAYXiIJ10==is)(sST");

	mail.send();
}
*/
