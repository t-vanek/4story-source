#include "StdAfx.h"
#include "TCompanionStats.h"
#include "TClientGame.h"
#include "Resource.h"


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

CTCompanionStats::CTCompanionStats( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
	: ITInnerFrame(pParent, pDesc, TCOMPANION_ATTRS)
{
	static DWORD dwAttrT[5] =
	{
		ID_PET_BONUS1T,
		ID_PET_BONUS2T,
		ID_PET_BONUS3T,
		ID_PET_BONUS4T,
		ID_PET_BONUS5T
	};

	static DWORD dwAttrV[5] =
	{
		ID_PET_BONUS1V,
		ID_PET_BONUS2V,
		ID_PET_BONUS3V,
		ID_PET_BONUS4V,
		ID_PET_BONUS5V
	};

	for( BYTE i=0; i < 5; ++i)
		m_pAttr[i] = FindKid( dwAttrT[i] );
	
	for( BYTE i=0; i < 5; ++i)
		m_pAttrV[i] = FindKid( dwAttrV[i] );
}

CTCompanionStats::~CTCompanionStats()
{
}
