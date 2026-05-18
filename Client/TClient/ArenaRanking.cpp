#include "stdafx.h"
#include "Resource.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "ArenaRanking.h"

static const BYTE BSize = 27 + 3;

ArenaRanking::ArenaRanking(TComponent* pParent, FRAMEDESC_SHAREDPTR pDesc)
	: CTClientUIBase(pParent, pDesc)
{
	m_Class[0] = (TImageList*) FindKid(12193);
	m_Level[0] = FindKid(18645);
	m_Name[0] = FindKid(863);

	for (BYTE i = 1; i < 3; ++i)
	{
		m_Class[i] = new TImageList(this, m_Class[0]->m_pDESC);
		m_Level[i] = new TComponent(this, m_Level[0]->m_pDESC);
		m_Name[i] = new TComponent(this, m_Name[0]->m_pDESC);

		m_Class[i]->m_id = GetUniqueID();
		m_Level[i]->m_id = GetUniqueID();
		m_Name[i]->m_id = GetUniqueID();

		m_Class[i]->MoveComponentBy(0, i * BSize);
		m_Level[i]->MoveComponentBy(0, i * BSize);
		m_Name[i]->MoveComponentBy(0, i * BSize);

		AddKid(m_Class[i]);
		AddKid(m_Level[i]);
		AddKid(m_Name[i]);
	}

	for (BYTE i = 3; i < 8; ++i)
	{
		m_Class[i] = new TImageList(this, m_Class[0]->m_pDESC);
		m_Level[i] = new TComponent(this, m_Level[0]->m_pDESC);
		m_Name[i] = new TComponent(this, m_Name[0]->m_pDESC);

		m_Class[i]->m_id = GetUniqueID();
		m_Level[i]->m_id = GetUniqueID();
		m_Name[i]->m_id = GetUniqueID();

		m_Class[i]->MoveComponentBy(0, 190);
		m_Level[i]->MoveComponentBy(0, 190);
		m_Name[i]->MoveComponentBy(0, 190);

		m_Class[i]->MoveComponentBy(0, (i - 3) * BSize);
		m_Level[i]->MoveComponentBy(0, (i - 3) * BSize);
		m_Name[i]->MoveComponentBy(0, (i - 3) * BSize);

		if (i > 4) //Very nice UI zemi
		{
			m_Class[i]->MoveComponentBy(0, 3 * (i - 4));
			m_Level[i]->MoveComponentBy(0, 3 * (i - 4));
			m_Name[i]->MoveComponentBy(0, 3 * (i - 4));
		}

		AddKid(m_Class[i]);
		AddKid(m_Level[i]);
		AddKid(m_Name[i]);
	}

	for (BYTE i = 0; i < ARENA_MAX_TEAM; ++i)
		m_Consecutive[i] = FindKid(26188 + i);
}

ArenaRanking::~ArenaRanking()
{

}

void ArenaRanking::AddTeam(const std::vector<ArenaPlayer>& Team, BYTE Wins)
{
	static BYTE Index = 0;

	switch (Index)
	{
	case 0:
	{
		for (BYTE i = 0; i < 3; ++i)
		{
			if (i < Team.size()) {
				m_Class[i]->SetCurImage(Team[i].m_Country);
				m_Level[i]->m_strText.Format("Level: %d", Team[i].m_Level);
				m_Name[i]->m_strText = Team[i].m_ChiefName;
			}
			else
			{
				m_Class[i]->SetCurImage(-1);
				m_Level[i]->m_strText.Empty();
				m_Name[i]->m_strText.Empty();
			}
		}
	}
	break;
	case 1:
	{
		for (BYTE i = 0; i < 5; ++i)
		{
			if (i < Team.size()) {
				m_Class[i + 3]->SetCurImage(Team[i].m_Country);
				m_Level[i + 3]->m_strText.Format("Level: %d", Team[i].m_Level);
				m_Name[i + 3]->m_strText = Team[i].m_ChiefName;
			}
			else
			{
				m_Class[i + 3]->SetCurImage(-1);
				m_Level[i + 3]->m_strText.Empty();
				m_Name[i + 3]->m_strText.Empty();
			}
		}
	}
	break;
	}

	m_Consecutive[Index]->m_strText.Format("Consecutive Wins : %d", Wins);

	if (Index == 1)
		Index = 0;
	else
		Index++;
}

void ArenaRanking::Update()
{
}

void ArenaRanking::Release()
{
	for (BYTE i = 0; i < 8; ++i)
	{
		m_Class[i]->SetCurImage(-1);
		m_Level[i]->m_strText.Empty();
		m_Name[i]->m_strText.Empty();
	}

	for (BYTE i = 0; i < 2; ++i)
		m_Consecutive[i]->m_strText.Empty();
}
