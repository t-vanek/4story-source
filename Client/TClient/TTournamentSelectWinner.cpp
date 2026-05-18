#include "stdafx.h"
#include "TTournamentSelectWinner.h"

#ifdef ADD_TOURNAMENT

CTTournamentSelectWinner::CTTournamentSelectWinner( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc)
: CTClientUIBase( pParent, pDesc )
{
	m_pPLAYERLIST = (TList*)FindKid( ID_CTRLINST_PLAYERLIST );
	m_pPLAYERLIST->AddString( "" );
}

CTTournamentSelectWinner::~CTTournamentSelectWinner()
{
}
#endif