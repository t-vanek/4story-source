#include "stdafx.h"
#include "TCompanionMiniUI.h"
#include "TClientGame.h"
#include "TCompanionInner.h"
#include "TClientCompanion.h"

CTCompanionMiniUI::CTCompanionMiniUI( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	m_pIcon = static_cast< TImageList* > ( FindKid(8553 ) );
	m_pItems[ 0 ] = static_cast< TImageList* > ( FindKid(11106 ) );
	m_pItems[ 1 ] = static_cast< TImageList* > ( FindKid(8613 ) );

	m_pCooldowns[ 0 ] = static_cast< TGauge* > ( FindKid( 26842 ) );
	m_pCooldowns[ 1 ] = static_cast< TGauge* > ( FindKid( 28080 ) );

	for( BYTE i = 0; i < 2; ++i )
		m_pCooldowns[ i ]->SetStyle( TGS_GROW_UP );

	static DWORD Pets[ 5 ] = {
		9681,
		9682,
		9683,
		9684,
		9685
	};
	
	for(BYTE i=0;i<5;++i)
		m_pPets[ i ] = FindKid( Pets[ i ] );
}

CTCompanionMiniUI::~CTCompanionMiniUI()
{

}

HRESULT CTCompanionMiniUI::Render(DWORD dwTickCount)
{
	CTClientGame* pGAME = CTClientGame::GetInstance();

	BOOL bShown = ( pGAME->GetMainChar()->IsDead() || pGAME->GetMainChar()->m_bGhost ) ? FALSE : TRUE;
	if( IsVisible() != bShown )
		ShowComponent( bShown );

	CTCompanionDlg* pInner = static_cast< CTCompanionDlg* >( pGAME->GetFrame( TFRAME_COMPANION ) );
	BYTE bCount = pInner->m_mapSpolecnici.size();
	for( BYTE i = 0; i < MAX_COMPANION_COUNT; ++i )
	{
		if( i < bCount )
			m_pPets[ i ]->ShowComponent( TRUE );

		else
			m_pPets[ i ]->ShowComponent( FALSE );
	}

	if( pInner->m_bSummonedSlot >= 0 && pInner->m_bSummonedSlot <= 4 )
	{
		CTClientCompanion* pCompanion = pInner->GetSelectedCompanion( pInner->m_bSummonedSlot );
		if( pCompanion )
		{
			LPTMONTEMP pTEMP = CTChart::FindTMONTEMP( pCompanion->GetMonID() );
			if( !pTEMP )
				return S_OK;

			m_pIcon->SetCurImage( pTEMP->m_wFaceIcon );
			LPTITEM pITEM[ 2 ];
			LPTITEMVISUAL pVISUALS[ 2 ];
			CTClientItem* pItem[ 2 ] = { NULL };

			for( BYTE i = 0; i < 2; ++i )
			{




				pItem[ i ] = pCompanion->m_pItem[ i ];
				if( !pItem[ i ] )
				{
					m_pItems[ i ]->SetCurImage( 0 );
					m_pCooldowns[ i ]->ShowComponent( FALSE );
					continue;
				}

				pITEM[ i ] = pItem[ i ]->GetTITEM();
				if( !pITEM[ i ] )
				{


					m_pItems[ i ]->SetCurImage( 0 );
					m_pCooldowns[ i ]->ShowComponent( FALSE );
					continue;
				}

				if( pItem[ i ]->GetTick( pITEM[ i ]->m_wDelayGroupID ) )
				{
					m_pCooldowns[ i ]->SetGauge( pItem[ i ]->GetTick( pITEM[ i ]->m_wDelayGroupID ), pITEM[ i ]->m_dwDelay );
					m_pCooldowns[ i ]->m_strText = CTClientGame::ToTimeString( pItem[ i ]->GetTick( pITEM[ i ]->m_wDelayGroupID ) );
					m_pCooldowns[ i ]->ShowComponent( TRUE );
				}
				else
					m_pCooldowns[ i ]->ShowComponent( FALSE );

				if( pITEM[ i ] )
				{
					pVISUALS[ i ] = CTChart::FindTITEMVISUAL( pITEM[ i ]->m_wVisual[ 0 ] );
					if (pVISUALS[i])
						m_pItems[ i ]->SetCurImage( pVISUALS[ i ]->m_wIcon );
				}
				else
					m_pItems[ i ]->SetCurImage( 0 );
			}
		}
	}
	else
	{
		LPTMONTEMP pTEMP = CTChart::FindTMONTEMP( 2553 );
		m_pIcon->SetCurImage( pTEMP->m_wFaceIcon );

		for( BYTE i = 0; i < 2; ++i )
		{
			m_pItems[ i ]->SetCurImage( 0 );
			m_pCooldowns[ i ]->SetGauge( 0, 1, FALSE );
			m_pCooldowns[ i ]->m_strText.Empty();
		}
	}

	return CTClientUIBase::Render(dwTickCount);
}
