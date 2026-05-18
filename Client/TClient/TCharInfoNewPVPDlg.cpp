#include "stdafx.h"
#include "TCharInfoNewPVPDlg.h"
#include "TClientGame.h"
#include "TCharInfoNewInner.h"

CTCharDuelsDlg::CTCharDuelsDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
: CTClientUIBase( pParent, pDesc)
{
	static DWORD dwButtons[] = {
		27997,
		27998,
		27999,
		28000,
		28001,
		28002,
		28003,
		28004,
		28005,
		28006
	};

	for(BYTE i=0;i<10;++i)
		m_pButtons[i] = (TButton*) FindKid(dwButtons[i]);

	m_pH_TITLE = FindKid( ID_CTRLINST_H_TITLE );

	m_pHL_TITLE = FindKid( ID_CTRLINST_H_L_TITLE );
	m_pHR_TITLE = FindKid( ID_CTRLINST_H_R_TITLE );;
	
	m_pH_COL[0][0] = FindKid( ID_CTRLINST_H_L1_COL );
	m_pH_COL[0][1] = FindKid( ID_CTRLINST_H_L2_COL );
	m_pH_COL[0][2] = FindKid( ID_CTRLINST_H_L3_COL );
	m_pH_COL[0][3] = FindKid( ID_CTRLINST_H_L4_COL );
	m_pH_COL[1][0] = FindKid( ID_CTRLINST_H_R1_COL );
	m_pH_COL[1][1] = FindKid( ID_CTRLINST_H_R2_COL );
	m_pH_COL[1][2] = FindKid( ID_CTRLINST_H_R3_COL );
	m_pH_COL[1][3] = FindKid( ID_CTRLINST_H_R4_COL );

	m_pH_VALUE[0][0] = FindKid( ID_CTRLINST_H_L1_VALUE );
	m_pH_VALUE[0][1] = FindKid( ID_CTRLINST_H_L2_VALUE );
	m_pH_VALUE[0][2] = FindKid( ID_CTRLINST_H_L3_VALUE );
	m_pH_VALUE[0][3] = FindKid( ID_CTRLINST_H_L4_VALUE );
	m_pH_VALUE[1][0] = FindKid( ID_CTRLINST_H_R1_VALUE );
	m_pH_VALUE[1][1] = FindKid( ID_CTRLINST_H_R2_VALUE );
	m_pH_VALUE[1][2] = FindKid( ID_CTRLINST_H_R3_VALUE );
	m_pH_VALUE[1][3] = FindKid( ID_CTRLINST_H_R4_VALUE );

	m_pM_L[0] = FindKid( ID_CTRLINST_M_L1 );
	m_pM_M[0] = FindKid( ID_CTRLINST_M_M1 );
	m_pM_R[0] = FindKid( ID_CTRLINST_M_R1 );
	m_pM_L[1] = FindKid( ID_CTRLINST_M_L2 );
	m_pM_M[1] = FindKid( ID_CTRLINST_M_M2 );
	m_pM_R[1] = FindKid( ID_CTRLINST_M_R2 );
	m_pM_L[2] = FindKid( ID_CTRLINST_M_L3 );
	m_pM_M[2] = FindKid( ID_CTRLINST_M_M3 );
	m_pM_R[2] = FindKid( ID_CTRLINST_M_R3 );
	m_pM_L[3] = FindKid( ID_CTRLINST_M_L4 );
	m_pM_M[3] = FindKid( ID_CTRLINST_M_M4 );
	m_pM_R[3] = FindKid( ID_CTRLINST_M_R4 );
	m_pM_L[4] = FindKid( ID_CTRLINST_M_L5 );
	m_pM_M[4] = FindKid( ID_CTRLINST_M_M5 );
	m_pM_R[4] = FindKid( ID_CTRLINST_M_R5 );
	m_pM_L[5] = FindKid( ID_CTRLINST_M_L6 );
	m_pM_M[5] = FindKid( ID_CTRLINST_M_M6 );
	m_pM_R[5] = FindKid( ID_CTRLINST_M_R6 );
	m_pM_L[6] = FindKid( ID_CTRLINST_M_L7 );
	m_pM_M[6] = FindKid( ID_CTRLINST_M_M7 );
	m_pM_R[6] = FindKid( ID_CTRLINST_M_R7 );

	m_pM_L[ 0 ]->m_strText = CTChart::LoadString( TSTR_CHARPVP_CLASS );

	for( INT i=0 ; i < 6 ; ++i )
		m_pM_L[ i+1 ]->m_strText = CTChart::LoadString( (TSTRING) CTClientGame::m_vTCLASSSTR[ i ] );

	m_pL_TITLE = FindKid( ID_CTRLINST_L_TITLE );
	m_pL_TITLE->m_strText = CTChart::LoadString( TSTR_CHARPVP_LATEST_RECORD );

	m_pL_COL1[0] = FindKid( ID_CTRLINST_L_L1_1 );
	m_pL_COL1[1] = FindKid( ID_CTRLINST_L_L2_1 );
	m_pL_COL1[2] = FindKid( ID_CTRLINST_L_L3_1 );
	m_pL_COL1[3] = FindKid( ID_CTRLINST_L_L4_1 );
	m_pL_COL1[4] = FindKid( ID_CTRLINST_L_L5_1 );
	m_pL_COL1[5] = FindKid( ID_CTRLINST_L_R1_1 );
	m_pL_COL1[6] = FindKid( ID_CTRLINST_L_R2_1 );
	m_pL_COL1[7] = FindKid( ID_CTRLINST_L_R3_1 );
	m_pL_COL1[8] = FindKid( ID_CTRLINST_L_R4_1 );
	m_pL_COL1[9] = FindKid( ID_CTRLINST_L_R5_1 );

	
	m_pL_COL2[0] = FindKid( ID_CTRLINST_L_L1_2 );
	m_pL_COL2[1] = FindKid( ID_CTRLINST_L_L2_2 );
	m_pL_COL2[2] = FindKid( ID_CTRLINST_L_L3_2 );
	m_pL_COL2[3] = FindKid( ID_CTRLINST_L_L4_2 );
	m_pL_COL2[4] = FindKid( ID_CTRLINST_L_L5_2 );
	m_pL_COL2[5] = FindKid( ID_CTRLINST_L_R1_2 );
	m_pL_COL2[6] = FindKid( ID_CTRLINST_L_R2_2 );
	m_pL_COL2[7] = FindKid( ID_CTRLINST_L_R3_2 );
	m_pL_COL2[8] = FindKid( ID_CTRLINST_L_R4_2 );
	m_pL_COL2[9] = FindKid( ID_CTRLINST_L_R5_2 );

	m_pL_COL3[0] = FindKid( ID_CTRLINST_L_L1_3 );
	m_pL_COL3[1] = FindKid( ID_CTRLINST_L_L2_3 );
	m_pL_COL3[2] = FindKid( ID_CTRLINST_L_L3_3 );
	m_pL_COL3[3] = FindKid( ID_CTRLINST_L_L4_3 );
	m_pL_COL3[4] = FindKid( ID_CTRLINST_L_L5_3 );
	m_pL_COL3[5] = FindKid( ID_CTRLINST_L_R1_3 );
	m_pL_COL3[6] = FindKid( ID_CTRLINST_L_R2_3 );
	m_pL_COL3[7] = FindKid( ID_CTRLINST_L_R3_3 );
	m_pL_COL3[8] = FindKid( ID_CTRLINST_L_R4_3 );
	m_pL_COL3[9] = FindKid( ID_CTRLINST_L_R5_3 );
	

	for( INT i=0 ; i < 10 ; ++i )
      m_pL_COL1[ i ]->m_strText.Format( "%d", i+1 );

	TComponent* pTexts[2];
	pTexts[0] = FindKid(28011);
	pTexts[1] = FindKid(28010);
	
	pTexts[0]->m_strText = "Score by Class";
	pTexts[1]->m_strText = "Battle Score Summary";


	m_bTabIndex = TINFO_PVP;
}

CTCharDuelsDlg::~CTCharDuelsDlg()
{
	{
		VTLATESTPVPINFO::iterator it, end;
		it = m_vLatestPvPInfo.begin();
		end = m_vLatestPvPInfo.end();

		for(; it != end ; ++it )
			delete (*it);

		m_vLatestPvPInfo.clear();
	}
}

void CTCharDuelsDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	//GetComponentPos( &CTCharInfoDlg::m_ptPOS);
	CTClientUIBase::OnLButtonUp( nFlags, pt );
}

void CTCharDuelsDlg::ShowComponent( BOOL bVisible)
{
	CTClientGame* pTGAME = CTClientGame::GetInstance();
	if (pTGAME->IsInBOWMap() || pTGAME->IsInBRMap())
		return;

	if (pTGAME->GetSession())
		pTGAME->GetSession()->SendCS_PVPRECORD_REQ( TRUE );

	CTClientUIBase::ShowComponent( bVisible );
}

ITDetailInfoPtr CTCharDuelsDlg::GetTInfoKey( const CPoint& point )
{
	ITDetailInfoPtr info;
	CTCharInfoNewDlg* pInfo = static_cast<CTCharInfoNewDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_CHARINFO_INNER));

	for( INT i=0 ; i < m_vLatestPvPInfo.size() ; ++i )
	{
		if( m_pL_COL1[ i ]->HitTest( point ) ||
			m_pL_COL2[ i ]->HitTest( point ) ||
			m_pL_COL3[ i ]->HitTest( point ))
		{
			CRect rc;
			pInfo->GetComponentRect(&rc);

			DWORD dwTitleColor = TNEXTLEV_TEXT_COLOR;

			info = CTDetailInfoManager::NewPvPInst(
				m_bTabIndex,
				m_vLatestPvPInfo[ i ]->m_strName,
				m_vLatestPvPInfo[ i ]->m_bWin,
				m_vLatestPvPInfo[ i ]->m_bClass,
				m_vLatestPvPInfo[ i ]->m_bLevel,
				m_vLatestPvPInfo[ i ]->m_dwPoint,
				m_vLatestPvPInfo[ i ]->m_dlDate,
				dwTitleColor,
				rc);

			info->SetDir(TRUE, TRUE, TRUE, TRUE);

			return info;
		}
	}

	return info;
}

void CTCharDuelsDlg::ResetData(
	BYTE bTabIndex,
	DWORD dwRankOrder,
	BYTE bRankPercent,
	DWORD* dwKillCountByClass,
	DWORD* dwDieCountByClass,
	VTLATESTPVPINFO vLatestPvPInfo,
	DWORD dwMonthRankOrder,
	BYTE bMonthRankPercent,
	WORD wMonthWin,
	WORD wMonthLose)
{
	{
		VTLATESTPVPINFO::iterator it, end;
		it = m_vLatestPvPInfo.begin();
		end = m_vLatestPvPInfo.end();

		for(; it != end ; ++it )
			delete (*it);

		m_vLatestPvPInfo.clear();
	}

	m_vLatestPvPInfo = vLatestPvPInfo;

	CTCharInfoNewDlg* pDlg = static_cast<CTCharInfoNewDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_CHARINFO_INNER));
	sort(m_vLatestPvPInfo.begin(), m_vLatestPvPInfo.end(), pDlg->SortByTime);

	DWORD dwTotalWin = 0;
	DWORD dwTotalLose = 0;

	for( INT i=0 ; i < TCLASS_COUNT ; ++i )
	{
		m_pM_M[ i+1 ]->m_strText.Format( "%d", dwKillCountByClass[ i ] );
		m_pM_R[ i+1 ]->m_strText.Format( "%d", dwDieCountByClass[ i ] );

		dwTotalWin += dwKillCountByClass[ i ];
		dwTotalLose += dwDieCountByClass[ i ];
	}

	ClearH();
	m_pH_TITLE->m_strText = CTChart::LoadString( TSTR_CHARPVP_PVP_SUMMARY );
	m_pHL_TITLE->m_strText = CString(" ");
	m_pHR_TITLE->m_strText = CString(" ");

	// ÃÑÀüÀû
	m_pH_COL[0][0]->m_strText = CTChart::LoadString( TSTR_CHARPVP_TOTAL_RECORD );
	m_pH_VALUE[0][0]->m_strText.Format( "%d", dwTotalWin + dwTotalLose );

	// ½Â·ü
	m_pH_COL[0][1]->m_strText = CTChart::LoadString( TSTR_CHARPVP_POFV );
	m_pH_VALUE[0][1]->m_strText.Format( "%.0f%%",
		dwTotalWin ? FLOAT(dwTotalWin) / FLOAT(dwTotalWin + dwTotalLose) * 100.0f : 0 );

	// ½Â
	m_pH_COL[1][0]->m_strText = CTChart::LoadString( TSTR_CHARPVP_WIN );
	m_pH_VALUE[1][0]->m_strText.Format( "%d", dwTotalWin );

	// ÆÐ
	m_pH_COL[1][1]->m_strText = CTChart::LoadString( TSTR_CHARPVP_LOSE );
	m_pH_VALUE[1][1]->m_strText.Format( "%d", dwTotalLose );

	m_pM_M[ 0 ]->m_strText = CTChart::LoadString( TSTR_CHARPVP_WIN );
	m_pM_R[ 0 ]->m_strText = CTChart::LoadString( TSTR_CHARPVP_LOSE );

	{
		for( size_t i=0 ; i < m_vLatestPvPInfo.size() ; ++i )
		{
			m_pL_COL2[ i ]->m_strText = m_vLatestPvPInfo[ i ]->m_strName;
			m_pL_COL3[ i ]->m_strText = m_vLatestPvPInfo[ i ]->m_bWin ?
				CTChart::LoadString( TSTR_CHARPVP_WIN ) : CTChart::LoadString( TSTR_CHARPVP_LOSE );
		}

		for(size_t n= m_vLatestPvPInfo.size(); n < 10 ; ++n )
		{
			m_pL_COL2[ n ]->m_strText.Empty();
			m_pL_COL3[ n ]->m_strText.Empty();
		}
	}
}

void CTCharDuelsDlg::ClearH()
{
	for( INT i=0 ; i < 2 ; ++i )
	{
		for( INT n=0 ; n < 4 ; ++n )
		{
			m_pH_COL[ i ][ n ]->m_strText.Empty();
			m_pH_VALUE[ i ][ n ]->m_strText.Empty();
		}
	}
}