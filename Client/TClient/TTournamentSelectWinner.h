#pragma once

#ifdef ADD_TOURNAMENT

class CTTournamentSelectWinner : public CTClientUIBase
{
public:
	CTTournamentSelectWinner( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTTournamentSelectWinner();

public:
	TList* m_pPLAYERLIST;
};

#endif