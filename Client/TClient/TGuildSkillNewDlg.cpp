#include "StdAfx.h"
#include "TFameRank.h"
#include "TGuildSkillNewDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TClient.h"
BYTE CTGuildSkillNewDlg::m_bTabIndex = TGUILD_SKILL;


CTGuildSkillNewDlg::CTGuildSkillNewDlg( TComponent *pParent, LP_FRAMEDESC pDesc)
	: ITInnerFrame(pParent, pDesc, TGUILD_SKILL)
{

	
}

CTGuildSkillNewDlg::~CTGuildSkillNewDlg()
{
	
}

HRESULT CTGuildSkillNewDlg::Render( DWORD dwTickCount)
{
	HRESULT hr = ITInnerFrame::Render(dwTickCount);

	return hr;
}

ITDetailInfoPtr CTGuildSkillNewDlg::GetTInfoKey( const CPoint& point )
{
	ITDetailInfoPtr pInfo;

	return pInfo;
}

