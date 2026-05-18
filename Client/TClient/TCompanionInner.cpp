#include "stdafx.h"
#include "TClientGame.h"
#include "TCompanionInner.h"
#include "TClientCompanion.h"
#include "TCompanionAttrs.h"
#include "TCompanionStats.h"
#include "CompStyleDlg.h"

CPoint pDefaultX = NULL;
DWORD  dwDefaultColor = 0;

#define TCOMPANION_EXHAUSTED_HP (2400)
#define TCOMPANION_NORMAL_HP	(10000)
#define TCOMPANION_HEALTHY_HP	(300000)

#define TCOLOR_COMPANION_EXHAUSTED		(0xFF8B0000)
#define TCOLOR_COMPANION_NORMAL			(0xFFFAFAD2)
#define TCOLOR_COMPANION_HEALTHY		(0xFF00EE76)

#define TSTR_COMPANION_ATKRATE			"Physical Attack Rate"		//11
#define TSTR_COMPANION_PHYDEF			"Evade"						//12
#define TSTR_COMPANION_PHYCRIT			"Physical Critical Rate"	//13
#define TSTR_COMPANION_CON				"Concetration"				//20
#define TSTR_COMPANION_MCRIT			"Magical Critical Rate"		//21
#define TSTR_COMPANION_HPB				"Life"						//50
#define TSTR_COMPANION_MPB				"Mana"						//51
#define TSTR_COMPANION_MATKRATE			"Magic Attack Rate"			//86
#define TSTR_COMPANION_MDEF				"Resistance"				//87
#define TSTR_COMPANION_HONOR			"Additional Honor"			//255

#define TSTR_COMPANION_EXP				"%d / %d"
#define TSTR_COMPANION_HP				"HP: %d"
#define TSTR_COMPANION_LEVEL			"Level: %d"
#define TSTR_COMPANION_DESPAWN			"Withdraw"
#define TSTR_COMPANION_SPAWN			"Call"
#define TSTR_COMPANION_EXHAUSTED		"Exhausted"
#define TSTR_COMPANION_NORMAL			"Normal"
#define TSTR_COMPANION_HEALTHY			"Healthy"
#define TSTR_COMPANION_DELETE			"Delete"
#define TSTR_COMPANION_TABONE			"Attribute"
#define TSTR_COMPANION_TABTWO			"Bonus"
#define TSTR_COMPANION_END				"Endurance"
#define TSTR_COMPANION_INT				"Intelligence"
#define TSTR_COMPANION_DEX				"Skill"
#define TSTR_COMPANION_WIS				"Wisdom"
#define TSTR_COMPANION_STR				"Strength"
#define TSTR_COMPANION_MEN				"Spirit"
#define TSTR_COMPANION_ATTRPOINT		"Status Points"
#define TSTR_COMPANION_LEVELUP			"Levelup"
#define TSTR_COMPANION_MAXEXP			"Maximum Experience"

#define ID_PET_LEVEL (0x000048D5)
#define ID_PET_TAB1 (0x000060D7)
#define ID_PET_TAB2 (0x000060D8)
#define ID_PET_SLOT1 (0x00002169)
#define ID_PET_SLOT2 (0x00006670)
#define ID_PET_HPBAR (0x0000666D)
#define ID_PET_EXPBAR (0x000067C2)
#define ID_PET_LEVELUP (0x00006692)
#define ID_PET_1 (0x000025D1)
#define ID_PET_2 (0x000025D2)
#define ID_PET_3 (0x000025D3)
#define ID_PET_4 (0x000025D4)
#define ID_PET_5 (0x000025D5)
#define ID_PET_EXP (0x00002E19)
#define ID_PET_HP (0x00006675) 
#define ID_PET_DELETE (0x00006690)
#define ID_PET_SLOTV1 (0x0000668E)
#define ID_PET_SLOTV2 (0x0000668F)
#define ID_PET_BONUSTAB_TEXT (0x00006811)
#define ID_PET_LEFTSWITCH (0x0000666F)
#define ID_PET_RIGHTSWITCH (0x00006670)
#define ID_PET_ARROWUP (0x00006671)
#define ID_PET_ARROWDN (0x0000666C)	
#define ID_PET_STRVAL (0x00006685)
#define ID_PET_INTVAL (0x00006686)
#define ID_PET_AGIVAL (0x00006687)
#define ID_PET_WISVAL (0x00006688)
#define ID_PET_ENDVAL (0x00006813)
#define ID_PET_WILLVAL (0x00006814)

#define ID_PET_STRTEXT (0x00006815)
#define ID_PET_INTTEXT (0x00006816) 
#define ID_PET_AGITEXT (0x00006817)
#define ID_PET_WISTEXT (0x00006818) 
#define ID_PET_ENDTEXT (0x00006819)
#define ID_PET_WILLTEXT (0x0000681A) 

#define ID_PET_STRBUT (0x0000667F)
#define ID_PET_INTBUT (0x00006680)
#define ID_PET_AGIBUT (0x00006681)
#define ID_PET_WISBUT (0x00006682)
#define ID_PET_ENDBUT (0x0000681B)
#define ID_PET_WILLBUT (0x0000681C)

#define ID_PET_STATUI (0x000064AF)
#define ID_PET_STATTEXT (0x0000667E)
#define ID_PET_STATVALUE (0x0000668B)
#define ID_PET_BONUS1T (0x00000FBB)
#define ID_PET_BONUS2T (0x00000FDF)
#define ID_PET_BONUS3T (0x00000FE0)
#define ID_PET_BONUS4T (0x00000FE1)
#define ID_PET_BONUS5T (0x00000FE2)
/**/
#define ID_PET_BONUS1V (0x00000FF3)
#define ID_PET_BONUS2V (0x00000FF2)
#define ID_PET_BONUS3V (0x00000FEF)
#define ID_PET_BONUS4V (0x00000FD3)
#define ID_PET_BONUS5V (0x00000FD4)
/**/
#define ID_FRAME_PETSTATS (0x00000481)
#define ID_PET_SUMMON (0x00006691)
#define ID_PET_STATUS  (0x00006806)
#define ID_PET_BONUS (0x00006812)
#define ID_PET_NAME (0x0000666E)
#define ID_PET_ICON (0x00006664)

CTCompanionDlg::CTCompanionDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
:	CTFrameGroupBase( pParent, pDesc, 26641 )
{
	static DWORD dwPET[ MAX_COMPANION_COUNT ] =
	{
		ID_PET_1,
		ID_PET_2,
		ID_PET_3,
		ID_PET_4,
		ID_PET_5
	};

	m_pTab1Shown = FindKid( ID_PET_STATUI ); 
	m_pStatPText = FindKid( ID_PET_STATTEXT );
	m_pStatPValue = FindKid( ID_PET_STATVALUE );
	m_pName = FindKid( ID_PET_NAME );
	m_pBonusName = FindKid( ID_PET_BONUS );
	m_pStatus = FindKid( ID_PET_STATUS );
	m_pHP = FindKid( ID_PET_HP );
	m_pExp = FindKid( ID_PET_EXP );
	m_pLevel = FindKid( ID_PET_LEVEL );
	m_pBonusT = FindKid( ID_PET_BONUSTAB_TEXT );

	m_pSummon = static_cast< TButton* >( FindKid( ID_PET_SUMMON ) );
	m_pDelete = static_cast< TButton* >( FindKid( ID_PET_DELETE ) );
	m_pLevelUp = static_cast< TButton* >( FindKid( ID_PET_LEVELUP ) );
	m_pLeftSwitch = static_cast< TButton* >( FindKid( ID_PET_LEFTSWITCH ) );
	m_pRightSwitch = static_cast< TButton* >( FindKid( ID_PET_RIGHTSWITCH ) );
	m_pArrowUP = static_cast< TButton* >( FindKid( ID_PET_ARROWUP ) ); 
	m_pChangeEffect = static_cast< TButton* >( FindKid( 27470 ) );

	m_pHPBar = static_cast< TGauge* >( FindKid( ID_PET_EXPBAR ) );
	m_pExpBar = static_cast< TGauge* >( FindKid( ID_PET_HPBAR ) );
	
	m_pItemTicks[ 0 ] = static_cast< TGauge* >( FindKid( 26543 ) );
	m_pItemTicks[ 1 ] = static_cast< TGauge* >( FindKid( 26542 ) );

	m_pSlot1T = static_cast< TImageList* >( FindKid( ID_PET_SLOTV2 ) );
	m_pSlot2T = static_cast< TImageList* >( FindKid( ID_PET_SLOTV1 ) );
	m_pIcon = static_cast< TImageList* >( FindKid( ID_PET_SLOT1 ) );

	for( BYTE i = 0; i < MAX_COMPANION_COUNT; ++i )

		m_pPet[ i ] = static_cast< TImageList* >( FindKid( dwPET[ i ] ) );

	for( BYTE i = 0; i < 2; ++i )
		m_pItemTicks[ i ]->SetStyle( TGS_GROW_UP );

	m_pLevelUp->ShowComponent( FALSE );
	m_bTab = TCOMPANION_STATS;
	m_pDelete->m_menu[ TNM_LCLICK ] = GM_COMPANION_DELETE;
	m_pChangeEffect->m_menu[ TNM_LCLICK ] = GM_COMPANION_EFFECT;
		dwDefaultColor = m_pChangeEffect->m_pFont->m_dwColor;

	m_pHPBar->SetStyle( TGS_GROW_UP );
	m_pExpBar->SetStyle( TGS_GROW_RIGHT );
	CPoint point;
	m_pArrowUP->GetComponentPos( &point );
	pDefaultX = point;
	m_bSummonedSlot = T_INVALID;
}

CTCompanionDlg::~CTCompanionDlg()
{
	m_bTab = TCOMPANION_STATS;
}

BOOL CTCompanionDlg::CanWithItemUI()
{
	return TRUE;
}

void CTCompanionDlg::ShowComponent( BOOL bVisible )
{	
	m_bCurSelSlot = m_bSummonedSlot > 0 && m_bSummonedSlot < 5 ? m_bSummonedSlot : 0;
	ITInnerFrame* pFrame = GetInnerFrame( TCOMPANION_STATS );

	if( pFrame )

		pFrame->RequestInfo();


	CTFrameGroupBase::ShowComponent( bVisible );
}

CTClientCompanion* CTCompanionDlg::GetSelectedCompanion( BYTE m_bSlot )
{
	if(IsCompanionEmpty())
		return NULL;

	Spolecnici::iterator finder = m_mapSpolecnici.find( m_bSlot ); //rework names pls //no ty

	if( finder != m_mapSpolecnici.end() )
		return (*finder).second;


	
	return NULL;
}

ITDetailInfoPtr	CTCompanionDlg::GetTInfoKey( const CPoint& pt )
{
	ITDetailInfoPtr pInfo;

	if( !IsVisible() )
		return pInfo;

	if( IsCompanionEmpty() )
		return pInfo;

	for( BYTE i=0;i<2;++i )
	{
		CTClientCompanion* m_pSelPet = GetSelectedCompanion( m_bCurSelSlot );
		if( !m_pSelPet )
			return pInfo;
		
		if( !m_pSelPet->m_pItem[ i ] )
			return pInfo;

		CTClientItem* m_pItem = m_pSelPet->m_pItem[ i ];
		if( ( i==0 && m_pItem && m_pSlot1T->HitTest( pt ) ) || ( i==1 && m_pItem && m_pSlot2T->HitTest( pt ) ) ) 
		{
			CRect rc;
			GetComponentRect(&rc);

			pInfo = CTDetailInfoManager::NewItemInst( m_pItem, rc );
		}
	}

	return pInfo;
}

void CTCompanionDlg::SelRight()
{
	if( m_bCurSelSlot + 1 <= m_mapSpolecnici.size() - 1 )
		m_bCurSelSlot++;
	else
		m_bCurSelSlot = 0;

	CCompStyleDlg* pStyleDlg = (CCompStyleDlg*) CTClientGame::GetInstance()->GetFrame(TFRAME_COMPSTYLE);
	if (pStyleDlg->IsVisible() && !pStyleDlg->m_bFinished)
		pStyleDlg->InitData(m_bCurSelSlot, pStyleDlg->m_wToMonID, pStyleDlg->m_bInvenID, pStyleDlg->m_bItemID);
}

void CTCompanionDlg::SelLeft()
{
	if( m_bCurSelSlot > 0 )
		m_bCurSelSlot--;
	else
		m_bCurSelSlot = m_mapSpolecnici.size() - 1;

	CCompStyleDlg* pStyleDlg = (CCompStyleDlg*) CTClientGame::GetInstance()->GetFrame(TFRAME_COMPSTYLE);
	if (pStyleDlg->IsVisible() && !pStyleDlg->m_bFinished)
		pStyleDlg->InitData(m_bCurSelSlot, pStyleDlg->m_wToMonID, pStyleDlg->m_bInvenID, pStyleDlg->m_bItemID);
}

DWORD CTCompanionDlg::GetCompanionMonID( BYTE m_bSlot )
{
	Spolecnici::iterator finder = m_mapSpolecnici.find( m_bSlot );
	if( finder != m_mapSpolecnici.end() )
		return (*finder).second->GetMonID();






	return 0;
}

BOOL CTCompanionDlg::IsCompanionEmpty()
{
	return m_mapSpolecnici.empty();
}

void CTCompanionDlg::Release()
{
	m_mapSpolecnici.clear();
	m_bCurSelSlot = 0;
	m_pArrowUP->ShowComponent( FALSE );
	m_bSummonedSlot = T_INVALID;

	m_pExp->m_strText = "0 / 0";
	m_pLevel->m_strText = "Lv: 0";
	m_pHP->m_strText = "0";
	m_pName->m_strText.Empty();
	m_pStatus->m_strText.Empty();
	m_pBonusName->m_strText.Empty();

	m_pHPBar->SetGauge( 0, 100, FALSE );
	m_pExpBar->SetGauge( 0, 100, FALSE );
	m_pIcon->SetCurImage( 0 );

	CTCompanionAttrs* pATTR = static_cast< CTCompanionAttrs* >( GetInnerFrame( TCOMPANION_STATS ) );
	pATTR->m_pStatPValue->m_strText = "0";
	for( BYTE j = 0; j < TCHARSTAT_COUNT; ++j )
	{
		pATTR->m_pStatB[ j ]->ShowComponent( FALSE );
		pATTR->m_pStatV[ j ]->m_strText = "0 + (0)";
	}

	CTCompanionStats* pSTATS = static_cast< CTCompanionStats* >( GetInnerFrame( TCOMPANION_ATTRS ) );
	for( BYTE i = 0; i < MAX_COMPANION_COUNT; ++i )

	{
		pSTATS->m_pAttr[ i ]->m_strText.Empty();
		pSTATS->m_pAttrV[ i ]->m_strText.Empty();
	}

	for(BYTE i = 0; i < MAX_COMPANION_COUNT; ++i )

		m_pPet[ i ]->SetCurImage( 0 );

	m_pSlot1T->SetCurImage( 0 );
	m_pSlot2T->SetCurImage( 0 );

	for(BYTE i = 0; i < 2; ++i )
	{
		m_pItemTicks[ i ]->SetGauge( 0, 1, FALSE );
		m_pItemTicks[ i ]->m_strText.Empty();
	}
}

void CTCompanionDlg::SetSummonedSlot( BYTE bSlot )
{
	m_bSummonedSlot = bSlot;
}

HRESULT CTCompanionDlg::Render(DWORD dwTickCount)
{
	if(IsVisible())
	{
		CTCompanionAttrs* pATTR = static_cast< CTCompanionAttrs* >( GetInnerFrame( TCOMPANION_STATS ) );
		CTCompanionStats* pSTATS = static_cast< CTCompanionStats* >( GetInnerFrame( TCOMPANION_ATTRS ) );



		TButton* pButton = static_cast< TButton* > ( FindKid( 26749 ) );
		TButton* pButton2 = static_cast< TButton* > ( FindKid( 28315 ) ); 
		TButton* pButton3 = static_cast< TButton* > ( FindKid( 28314 ) ); 




		pButton->ShowComponent( FALSE );
		pButton2->ShowComponent( FALSE );
		pButton3->ShowComponent( FALSE );


		for( BYTE i = 0; i < m_mapSpolecnici.size(); ++i )
			m_pPet[ i ]->SetCurImage( 2 );
		for(size_t i = m_mapSpolecnici.size(); i < MAX_COMPANION_COUNT; ++i )

			m_pPet[ i ]->SetCurImage( 0 );

		if( IsCompanionEmpty() )
		{
			m_pLevelUp->ShowComponent( FALSE );
			m_pSummon->ShowComponent( FALSE );
			m_pDelete->ShowComponent( FALSE );
			m_pArrowUP->ShowComponent( FALSE );

			for(BYTE i = 0; i < TCHARSTAT_COUNT; ++i )

				pATTR->m_pStatB[ i ]->ShowComponent( FALSE );

			m_pChangeEffect->m_pFont->m_dwColor = m_pSummon->m_pFont->m_dwColor = 0xFF696969;
			m_pChangeEffect->EnableComponent( FALSE );
		}

		if( m_mapSpolecnici.size() <= 1 )
		{
			m_pLeftSwitch->ShowComponent( FALSE );
			m_pRightSwitch->ShowComponent( FALSE );
		}

		if( !IsCompanionEmpty() )
		{
			CTClientCompanion* m_pSelPet = GetSelectedCompanion( m_bCurSelSlot );
			if( m_pSelPet )
			{
				BOOL m_bShowLUP = m_pSelPet->GetExp() >= m_pSelPet->GetNextExp();
				BOOL m_bShowB = m_pSelPet->GetExp() < m_pSelPet->GetNextExp();

				if( m_bSummonedSlot >= 0 && m_bSummonedSlot <= 4 )
					m_pPet[ m_bSummonedSlot ]->SetCurImage( 1 );

				if( m_bSummonedSlot == m_bCurSelSlot )
					m_pSummon->m_strText = "Withdraw";
				else
					m_pSummon->m_strText = "Summon";

				m_pArrowUP->ShowComponent( TRUE );
				m_pExp->m_strText.Format( "EP: %d/%d", m_pSelPet->GetExp(), m_pSelPet->GetNextExp() );
				m_pLevel->m_strText.Format( "Lv: %d", m_pSelPet->GetLevel() );
				m_pHP->m_strText.Format( "%d", m_pSelPet->GetLife() );

				m_pHPBar->SetGauge( m_pSelPet->GetLife(), 11000, FALSE );
				m_pExpBar->SetGauge( m_pSelPet->GetExp(), m_pSelPet->GetNextExp(), FALSE );

				if( m_pSelPet->GetLife() < 2400 )
					m_pChangeEffect->m_pFont->m_dwColor = m_pSummon->m_pFont->m_dwColor = 0xFF696969;
				else
					m_pChangeEffect->m_pFont->m_dwColor = m_pSummon->m_pFont->m_dwColor = m_pDelete->m_pFont->m_dwColor = dwDefaultColor;

				LPTMONTEMP pTMON = CTChart::FindTMONTEMP( m_pSelPet->GetMonID() );
				if( pTMON )
				{
					m_pName->m_strText.Format( "%s (%s)", m_pSelPet->GetCompanionName(), pTMON->m_strNAME);
					m_pIcon->SetCurImage( pTMON->m_wFaceIcon );
				}

				if( m_pSelPet->GetLife() < TCOMPANION_EXHAUSTED_HP )
				{
					m_pStatus->m_strText = TSTR_COMPANION_EXHAUSTED;
					m_pStatus->SetTextClr( TCOLOR_COMPANION_EXHAUSTED );
				}
				else if( m_pSelPet->GetLife() <= TCOMPANION_NORMAL_HP )
				{
					m_pStatus->m_strText = TSTR_COMPANION_NORMAL;
					m_pStatus->SetTextClr( 0xFFEDEDED );
				}
				else if( m_pSelPet->GetLife() <= TCOMPANION_HEALTHY_HP )
				{
					m_pStatus->m_strText = TSTR_COMPANION_HEALTHY;
					m_pStatus->SetTextClr( THPTEXT_COLOR );
				}

				m_pLevelUp->ShowComponent( m_bShowLUP );
				m_pSummon->ShowComponent( m_bShowB );
	if( m_pSelPet->GetLife() >= 2400 != m_pSummon->IsEnable() )
					m_pSummon->EnableComponent( m_pSelPet->GetLife() >= 2400 );

				m_pDelete->ShowComponent( m_bShowB );

				BOOL bShowCEffect = m_bSummonedSlot == m_bCurSelSlot && m_pSelPet && m_pSelPet->GetMonID() != 21000;
				if( !bShowCEffect )
					m_pChangeEffect->m_pFont->m_dwColor = 0xFF696969;
				else
					m_pChangeEffect->m_pFont->m_dwColor = dwDefaultColor;

				if( m_pChangeEffect->IsEnable() != bShowCEffect )
					m_pChangeEffect->EnableComponent( bShowCEffect );

				pATTR->m_pStatPValue->m_strText.Format( "%d", m_pSelPet->GetAttrPoint() );
				BYTE* wCharAttr = m_pSelPet->GetCharATTR();

				for( BYTE j = 0; j < TCHARSTAT_COUNT; ++j )
				{
					if( m_pSelPet && m_pSelPet->GetAttrPoint() && wCharAttr[ j ] < 30 )
						pATTR->m_pStatB[ j ]->ShowComponent( TRUE );
					else
						pATTR->m_pStatB[ j ]->ShowComponent( FALSE );

					BYTE bPlusAdder = 0;
					for( BYTE x = 0; x < m_mapSpolecnici.size(); ++x )
					{
						CTClientCompanion* pCompanion = GetSelectedCompanion( x );
						bPlusAdder += 1;

						if( pCompanion->GetLevel() >= 11 && pCompanion->GetLife() >= 2400 )
							bPlusAdder += MAX_COMPANION_COUNT;


					}

					pATTR->m_pStatV[ j ]->m_strText.Format( "%d + (%d)", wCharAttr[ j ], bPlusAdder );
				}

				CString strBonus;
				CString strAttr;
				GetAttrString( strAttr, m_pSelPet );

				if( m_pSelPet->GetBonusID() != 88 )
					strBonus.Format( "%s +%g", strAttr, m_pSelPet->GetBonusValue() );
				else
					strBonus.Format( "%s +%.1f%%", strAttr, m_pSelPet->GetBonusValue() );

				m_pBonusName->m_strText = strBonus;

				for(size_t i = 0; i < m_mapSpolecnici.size(); ++i )
				{
					CTClientCompanion* pCompanion = GetSelectedCompanion( i );

					GetAttrString( pSTATS->m_pAttr[ i ]->m_strText, pCompanion );
					pSTATS->m_pAttrV[ i ]->m_strText.Format( pCompanion->GetBonusID() == 88 ? "+%.1f%%" : "+%g", pCompanion->GetBonusValue() ); //nevieme kade isù
					if( ( m_bSummonedSlot < 0 || m_bSummonedSlot > 4 ) || pCompanion->GetLevel() <= 11 || pCompanion->GetLife() < 2400 )
						pSTATS->m_pAttrV[ i ]->SetTextClr( TCOLOR_COMPANION_EXHAUSTED );
					if( m_bSummonedSlot >= 0 && m_bSummonedSlot <= 4 )
						pSTATS->m_pAttrV[ m_bSummonedSlot ]->SetTextClr( TCOLOR_COMPANION_HEALTHY );
					if( m_bSummonedSlot >= 0 && m_bSummonedSlot <= 4 && pCompanion->GetLevel() >= 11 && pCompanion->GetLife() >= 2400 )
						pSTATS->m_pAttrV[ i ]->SetTextClr( TCOLOR_COMPANION_HEALTHY );
				}

				for(size_t i = m_mapSpolecnici.size(); i < MAX_COMPANION_COUNT; ++i )

				{
					pSTATS->m_pAttr[ i ]->m_strText.Empty();
					pSTATS->m_pAttrV[ i ]->m_strText.Empty();
				}

				CTClientItem* m_pItems[ 2 ];
				for(BYTE i = 0; i < 2; ++i )
					m_pItems[ i ] = m_pSelPet->m_pItem[ i ];

				LPTITEM pFITEM = m_pItems[ 0 ]->GetTITEM();
				if( pFITEM )
				{
					LPTITEMVISUAL pVISUAL = CTChart::FindTITEMVISUAL( pFITEM->m_wVisual[ 0 ] );
					if(pVISUAL)
					{
						m_pSlot1T->SetCurImage( pVISUAL->m_wIcon );

						if(m_pItems[ 0 ]->GetTick(pFITEM->m_wDelayGroupID))
						{
							m_pItemTicks[ 0 ]->SetGauge( m_pItems[ 0 ]->GetTick( pFITEM->m_wDelayGroupID ), pFITEM->m_dwDelay, FALSE );
							m_pItemTicks[ 0 ]->m_strText = CTClientGame::ToTimeString( m_pItems[ 0 ]->GetTick( pFITEM->m_wDelayGroupID ) );
							m_pItemTicks[ 0 ]->ShowComponent( TRUE );
						}
						else
							m_pItemTicks[ 0 ]->ShowComponent( FALSE );
					}
				}
				else
				{
					m_pItemTicks[ 0 ]->SetGauge( 0, 1, FALSE );
					m_pItemTicks[ 0 ]->m_strText.Empty();
					m_pSlot1T->SetCurImage( 0 );
				}

				LPTITEM pSITEM = m_pItems[ 1 ]->GetTITEM();
				if( pSITEM )
				{
					LPTITEMVISUAL pSVISUAL = CTChart::FindTITEMVISUAL( pSITEM->m_wVisual[ 0 ] );
					if( pSVISUAL )
					{
						if( m_pItems[ 1 ]->GetTick( pSITEM->m_wDelayGroupID ) )
						{
							m_pItemTicks[ 1 ]->SetGauge( m_pItems[ 1 ]->GetTick( pSITEM->m_wDelayGroupID ), pSITEM->m_dwDelay, FALSE );
							m_pItemTicks[ 1 ]->m_strText = CTClientGame::ToTimeString( m_pItems[ 1 ]->GetTick( pSITEM->m_wDelayGroupID ) );
							m_pItemTicks[ 1 ]->ShowComponent( TRUE );
						}
						else
							m_pItemTicks[1]->ShowComponent( FALSE );

						m_pSlot2T->SetCurImage( pSVISUAL->m_wIcon );
					}
				}
				else
				{
					m_pItemTicks[ 1 ]->SetGauge( 0, 1, FALSE );
					m_pItemTicks[ 1 ]->m_strText.Empty();
					m_pSlot2T->SetCurImage( 0 );
				}

			}
		}

		m_pArrowUP->MoveComponent( CPoint( pDefaultX.x + ( m_bCurSelSlot * 16 ), pDefaultX.y ) );
	}

	return CTFrameGroupBase::Render(dwTickCount);
}

void CTCompanionDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	if(IsVisible() && !IsCompanionEmpty())
	{
		if( m_pLeftSwitch->HitTest( pt ) )
			SelLeft();
		else if( m_pRightSwitch->HitTest( pt ) )
			SelRight();
		else if( m_pSummon->HitTest(pt) && m_pSummon->IsEnable() )
		{
			if( GetTickCount() - m_dwTick < 200 )
				return;
			else
				m_dwTick = GetTickCount();
			CTClientCompanion* pSelPet = GetSelectedCompanion( m_bCurSelSlot );
			if( m_bSummonedSlot == m_bCurSelSlot )

				CTClientGame::GetInstance()->GetSession()->SendCS_COMPANIONCANCEL_REQ();



			else if( pSelPet && ( m_bSummonedSlot == T_INVALID || m_bSummonedSlot != m_bCurSelSlot ) )
				CTClientGame::GetInstance()->GetSession()->SendCS_COMPANIONRECALL_REQ( pSelPet->GetMonID(), m_bCurSelSlot );
		}
		else if( m_pLevelUp->HitTest( pt ) )
		{
			CTClientCompanion* pSelPet = GetSelectedCompanion( m_bCurSelSlot );
			if( pSelPet )
				CTClientGame::GetInstance()->GetSession()->SendCS_COMPANIONLUP_REQ( m_bCurSelSlot );
		}
	}

	CTFrameGroupBase::OnLButtonUp(nFlags,pt);
}

void CTCompanionDlg::GetAttrString(CString &strAttr, CTClientCompanion* pCompanion)
{
	switch(pCompanion->GetBonusID())
	{
	case 11: strAttr = TSTR_COMPANION_ATKRATE; break;
	case 12: strAttr = TSTR_COMPANION_PHYDEF; break;
	case 13: strAttr = TSTR_COMPANION_PHYCRIT; break;
	case 20: strAttr = TSTR_COMPANION_CON; break;
	case 21: strAttr = TSTR_COMPANION_MCRIT; break;
	case 50: strAttr = TSTR_COMPANION_HPB; break;
	case 51: strAttr = TSTR_COMPANION_MPB; break;
	case 86: strAttr = TSTR_COMPANION_MATKRATE; break;
	case 87: strAttr = TSTR_COMPANION_MDEF; break;
	case 88: strAttr = TSTR_COMPANION_HONOR; break;
	default: strAttr.Empty();
	}
}

BYTE CTCompanionDlg::OnBeginDrag( LPTDRAG pDRAG, CPoint point)
{
	BYTE bSlot = T_INVALID;
	if( m_pSlot1T->HitTest( point ) && m_pSlot1T->GetCurImage() > 0 )
		bSlot = 0;
	else if( m_pSlot2T->HitTest( point ) && m_pSlot2T->GetCurImage() > 0 )
		bSlot = 1;

	if( bSlot == 0 || bSlot == 1 )
	{
		if( pDRAG )
		{
			CPoint pt;

			pDRAG->m_pIMAGE = new TImageList(
				NULL,
				bSlot == 0 ? *m_pSlot1T : *m_pSlot2T);

			pDRAG->m_pIMAGE->SetCurImage(bSlot == 0 ? m_pSlot1T->GetCurImage() : m_pSlot2T->GetCurImage());
			pDRAG->m_pIMAGE->m_strText = NAME_NULL;
			pDRAG->m_dwParam = (DWORD) bSlot;

			if(bSlot == 0)
			{
				m_pSlot1T->GetComponentPos(&pt);
				m_pSlot1T->ComponentToScreen(&pt);
				m_pSlot1T->m_strText.Empty();
			}
			else
			{
				m_pSlot2T->GetComponentPos(&pt);
				m_pSlot2T->ComponentToScreen(&pt);
				m_pSlot2T->m_strText.Empty();
			}

			pDRAG->m_pIMAGE->ShowComponent(TRUE);
			pDRAG->m_pIMAGE->MoveComponent(pt);
		}

		return TRUE;
	}

	return FALSE;
}

TDROPINFO CTCompanionDlg::OnDrop(CPoint point)
{
	TDROPINFO vResult;

	if( m_pSlot1T->HitTest(point) || m_pSlot2T->HitTest(point) )
	{
		BYTE bSlot = T_INVALID;
		if(m_pSlot1T->HitTest(point) && m_pSlot1T->GetCurImage() > 0)
			bSlot = 0;
		else if(m_pSlot2T->HitTest(point) && m_pSlot2T->GetCurImage() > 0)
			bSlot = 1;

		vResult.m_bDrop = TRUE;
		vResult.m_bSlotID = bSlot;
		return vResult;
	}
	
	return vResult;
}