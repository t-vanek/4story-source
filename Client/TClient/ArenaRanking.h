#pragma once

#define ARENA_MAX_TEAM		2

#include "TArenaReg.h"

class ArenaRanking : public CTClientUIBase
{
private:
	TImageList* m_Class[3 + 5];
	TComponent* m_Level[3 + 5];
	TComponent* m_Name[3 + 5];

	TComponent* m_Consecutive[ARENA_MAX_TEAM];
public:
	ArenaRanking(TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~ArenaRanking();
public:
	void AddTeam(const std::vector<ArenaPlayer>& Team, BYTE Win);
	void Update();
	void Release();
};
