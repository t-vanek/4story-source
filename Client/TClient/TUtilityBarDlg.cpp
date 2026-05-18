#include "StdAfx.h"
#include "TUtilityBarDlg.h"
#include "TClientGame.h"

#define ID_BUTTON_EX (0x000065D7)
#define ID_BUTTON_N1 (0x000065CF)
#define ID_BUTTON_N2 (0x000065D0)
#define ID_BUTTON_N3 (0x000065D1)
#define ID_BUTTON_N4 (0x000065D2)
#define ID_BUTTON_N5 (0x000065D3)
#define ID_BUTTON_N6 (0x000065D4)
#define ID_BUTTON_N7 (0x000065D5)
#define ID_BUTTON_N8 (0x000065D6)
#define ID_BUTTON_N9 (0x0000680F)
#define ID_IMAGE_USELESS (0x0000680E)

CTUtilityBarDlg::CTUtilityBarDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
	: CTClientUIBase(pParent, pDesc)
{
	static DWORD dwHiden[9] =
	{
		ID_BUTTON_N1,
		ID_BUTTON_N2,
		ID_BUTTON_N3,
		ID_BUTTON_N4,
		ID_BUTTON_N5,
		ID_BUTTON_N6,
		ID_BUTTON_N7,
		ID_BUTTON_N8,
		ID_BUTTON_N9
	};

	for(BYTE i=0;i<9;++i)
		m_pButIdx[i] = static_cast<TButton*>(FindKid(dwHiden[i]));

	m_pLock = FindKid(ID_IMAGE_USELESS);
    m_pButEx = static_cast<TButton*>(FindKid(ID_BUTTON_EX));

	GAMEOPTION& GameOption = CTClientGame::GetInstance()->GetDevice()->m_option;

	bShownBar = TRUE;
}

CTUtilityBarDlg::~CTUtilityBarDlg()
{

}

HRESULT CTUtilityBarDlg::Render(DWORD dwTickCount)
{
	bShownAll = CTClientGame::m_vTOPTION.m_bUIBar;

	for (BYTE i = 0; i < 9; ++i)
	{
		if (bShownBar && bShownAll)
			m_pButIdx[i]->ShowComponent(TRUE);
		else
			m_pButIdx[i]->ShowComponent(FALSE);

		if (!bShownAll)
			m_pButEx->ShowComponent(FALSE);
		else
			m_pButEx->ShowComponent(TRUE);
	}

	m_pButIdx[ 8 ]->ShowComponent( FALSE );

	m_pLock->ShowComponent(!CTClientUIBase::m_bDragLock);

	return CTClientUIBase::Render(dwTickCount);
}
void CTUtilityBarDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	if(!IsVisible() || CTClientGame::IsInBOWMap() || CTClientGame::IsInBRMap())
		return;

	BYTE m_bChoosen = T_INVALID;
	CTClientGame* pGame = CTClientGame::GetInstance();

	if(m_pButEx->HitTest(pt))
		bShownBar = !bShownBar;

	for(BYTE i=0;i<9;++i)
	{
		if(m_pButIdx[i]->HitTest(pt))
		{
			m_bChoosen = i;
			if(m_bChoosen != T_INVALID)
				switch(m_bChoosen)
			{
				case 0:
					{
						pGame->PickNext(TPICK_COUNT);
						pGame->EnableUI(TFRAME_ITEM_UP);
					}
					break;
				case 1:
					{
						pGame->PickNext(TPICK_COUNT); 
						pGame->EnableUI(TFRAME_ITEM_REFINE);
					}
					break;
				case 2:
					{
						pGame->PickNext(TPICK_COUNT); 
						pGame->EnableUI(TFRAME_ITEM_REPAIR);
					}
					break;
				case 3:
					{
						pGame->PickNext(TPICK_COUNT); 
						CTCabinetDlg* pCabDlg = static_cast<CTCabinetDlg*>(pGame->GetFrame(TFRAME_CABINET));
						pCabDlg->RequestShowComponent();
					}
					break;
				case 4:
					{
						TTERMPOS vTAUTOPATH;

						ZeroMemory( &vTAUTOPATH, sizeof(TTERMPOS));
						switch(pGame->GetMainChar()->m_bContryID)
						{
						case 0:
							{
								vTAUTOPATH.m_dwMapID = 0;
								vTAUTOPATH.m_fPosX = 1584.72f;
								vTAUTOPATH.m_fPosY = 0;
								vTAUTOPATH.m_fPosZ = 4335.88f;//8
								vTAUTOPATH.m_dwNpcID = 20789;
								pGame->m_dwMoveGM = GM_NPC_TRADE;
							}
							break;
						case 1:
							{
								vTAUTOPATH.m_dwMapID = 0;
								vTAUTOPATH.m_fPosX = 6788.74f;
								vTAUTOPATH.m_fPosY = 0;
								vTAUTOPATH.m_fPosZ = 4910.31f;
								vTAUTOPATH.m_dwNpcID = 20841;
								pGame->m_dwMoveGM = GM_NPC_TRADE;
							}
							break;
						default:
							{
								vTAUTOPATH.m_dwMapID = 0;
								vTAUTOPATH.m_fPosX = 3749.84f;
								vTAUTOPATH.m_fPosY = 0;
								vTAUTOPATH.m_fPosZ = 242.31f;
								vTAUTOPATH.m_dwNpcID = 26153;
								pGame->m_dwMoveGM = GM_NPC_TRADE;
							}
							break;
						}
						pGame->DoTAUTOPATH(&vTAUTOPATH);
						CString strMsg = "Walking to Auctioneer";
						CTMainUI* pMain = static_cast<CTMainUI*> (pGame->GetFrame(TFRAME_MAIN));
						pMain->ResetPositionMsg(strMsg);
					}
					break;
				case 5:
                     pGame->OnGM_UB_SHOP_CANCEL();
					break;
				case 6:
					{
						pGame->PickNext(TPICK_COUNT); 
						pGame->EnableUI(TFRAME_ITEM_GAMBLE);
					}
					break;
				case 7:
					{
						TTERMPOS vTAUTOPATH;

						ZeroMemory( &vTAUTOPATH, sizeof(TTERMPOS));
						switch(pGame->GetMainChar()->m_bContryID)
						{
						case 0:
							{
								vTAUTOPATH.m_dwMapID = 0;
								vTAUTOPATH.m_fPosX = 1535.87f;
								vTAUTOPATH.m_fPosY = 0;
								vTAUTOPATH.m_fPosZ = 4264.08f;
								vTAUTOPATH.m_dwNpcID = 20789;
							}
							break;
						case 1:
							{
								vTAUTOPATH.m_dwMapID = 0;
								vTAUTOPATH.m_fPosX = 6770.83f;
								vTAUTOPATH.m_fPosY = 0;
								vTAUTOPATH.m_fPosZ = 4905.38f;
								vTAUTOPATH.m_dwNpcID = 20841;
								pGame->m_dwMoveGM = GM_NPC_TRADE;
							}
							break;
						default:
							break;
						}
						pGame->DoTAUTOPATH(&vTAUTOPATH);
						CString strMsg = "Walking to Mission Portal";
						CTMainUI* pMain = static_cast<CTMainUI*> (pGame->GetFrame(TFRAME_MAIN));
						pMain->ResetPositionMsg(strMsg);
					}
					break;
			}
		}

	}

	CTClientUIBase::OnLButtonUp(nFlags,pt);
}

BOOL CTUtilityBarDlg::CanWithItemUI()
{
	return TRUE;
}