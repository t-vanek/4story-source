#include "StdAfx.h"
#include "Resource.h"
#include "TClientGame.h"

CTNPCTalkDlg::CTNPCTalkDlg( TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc, CTClientChar* pHost, TCMLParser* pParser)
: CQuestNewDlg( pParent, pDesc, pHost)
{
	m_pACCEPT->m_menu[TNM_LCLICK] = 0;
	m_pREFUSE->m_menu[TNM_LCLICK] = 0;

	m_dwCompleteID = TSTR_COMPLETE;
	m_dwAcceptID = TSTR_ACCEPT;
	m_bPrintMSG = FALSE;

	m_vTDEFTALK.clear();
	m_bSND = FALSE;
	m_pTFollowDlg = NULL;
}

CTNPCTalkDlg::~CTNPCTalkDlg()
{
}

HRESULT CTNPCTalkDlg::Render( DWORD dwTickCount)
{
	if(!m_bVisible)
		return S_OK;
	/*
	if(GetCurSelTQuest() != &m_BQuest )
	{
		m_strNPCTalk = "";
		m_strAnswerWhenNPCTalk = "";

		BOOL bDEFTALK = TRUE;

		if(GetCurTQuest())
		{
			bDEFTALK = FALSE;

			if(GetCurTQuest()->m_bType == QT_MISSION )
			{
				CTClientGame* pTGAME = CTClientGame::GetInstance(); 
				CTClientObjBase* pTARGET = pTGAME->GetTargetObj();
				if( pTARGET )
				{
					MAPDWORD::iterator findCOND = pTARGET->m_mapTQUESTCOND.find(GetCurTQuest()->m_dwID);
					if( findCOND != pTARGET->m_mapTQUESTCOND.end() )
					{
						if( findCOND->second != QCT_NONE )
							bDEFTALK = TRUE;
					}
				}
			}
		}
		
		if(bDEFTALK)
			DefaultTalk();
		else
			Talk(GetCurTQuest()->m_strTriggerMSG);
			
		m_BQuest = *GetCurSelTQuest();
	}
	*/
	return CQuestNewDlg::Render(dwTickCount);
}

void CTNPCTalkDlg::DefaultTalk()
{
	/*
	if(!m_vTDEFTALK.empty())
	{
		LPTQUEST pTALK = m_vTDEFTALK[rand() % INT(m_vTDEFTALK.size())];

		m_strNPCTitle = pTALK->m_strNPCName;
		Talk(pTALK->m_strTriggerMSG);
	}
	*/
}

void CTNPCTalkDlg::ShowComponent( BOOL bVisible)
{
	CQuestNewDlg::ShowComponent(bVisible);
}

void CTNPCTalkDlg::Talk( CString strTALK)
{
	m_RightSide.Reset();
	m_nDesiredRightScrollY = 0;
	m_nCurRightScrollY = 0;

	m_strNPCTalk = strTALK;
	m_strAnswerWhenNPCTalk.Empty();

	CString strHeader = GetSpeakerString(m_strNPCTitle);
	m_RightSide.DialogueMsg( strHeader, m_strNPCTalk);

	m_pACCEPT->m_strText = "Next";
	m_pREFUSE->m_strText = "Run";

	m_RightSide.ReAlign(0);
}

void CTNPCTalkDlg::Talk( CString strTALK, CString strANSWER)
{
	m_strNPCTalk = strTALK;
	m_strAnswerWhenNPCTalk = strANSWER;

	m_RightSide.Reset();
	m_nDesiredRightScrollY = 0;
	m_nCurRightScrollY = 0;

	CString strHeader = GetSpeakerString(m_pHost->GetName());
	m_RightSide.DialogueMsg( strHeader, m_strAnswerWhenNPCTalk);

	strHeader = GetSpeakerString(m_strNPCTitle);
	m_RightSide.DialogueMsg( strHeader, m_strNPCTalk);

	m_pACCEPT->m_strText = "Next";
	m_pREFUSE->m_strText = "Run";

	m_RightSide.ReAlign(0);
}

CQuest* CTNPCTalkDlg::GetCurSelTQuest()
{
	return m_Quest;
}

LPTQUEST CTNPCTalkDlg::GetCurTQuest()
{
	return m_pTQUEST;
}

LPTQUEST CTNPCTalkDlg::GetCurTMission()
{
	if(!m_pTQUEST)
		return NULL;

	switch(m_pTQUEST->m_bType)
	{
	case QT_COMPLETE	: return CTChart::FindTMISSION(m_pTQUEST);
	case QT_GUILD		:
	case QT_MISSION		: return m_pTQUEST;
	}

	return NULL;
}

ITDetailInfoPtr CTNPCTalkDlg::GetTInfoKey( const CPoint& point )
{
	return CQuestNewDlg::GetTInfoKey(point);
}
