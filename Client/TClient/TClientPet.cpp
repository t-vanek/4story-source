#include "Stdafx.h"
#include "TClientPet.h"
#include "TClientGame.h"
#include "TPetManageDlg.h"
// =======================================================
CTClientPet::CTClientPet()
	: CTClientRecall(), m_wPetID(0), m_pPetTemp(NULL), m_pTakeUpChar(NULL)
{
	m_bCanSelected = FALSE;
}
// -------------------------------------------------------
CTClientPet::~CTClientPet()
{
	if( m_dwTakeUpPivot )
	{
		MAPOBJECT::iterator itr = m_mapEQUIP.find(m_dwTakeUpPivot);
		if( itr != m_mapEQUIP.end() )
			m_mapEQUIP.erase(itr);
	}
	if( m_pTakeUpChar )
		m_pTakeUpChar->SetRidingPet(NULL);
}
// =======================================================
void CTClientPet::SetPetInfo(const CString& strName, WORD wPetID, const CTime& tEndTime, BYTE m_bEffect)
{
	m_pPetTemp = CTChart::FindTPETTEMP(wPetID);

	if (!m_pPetTemp)
		return;

	m_strPetName = strName;

	m_wPetID = wPetID;
	m_tPetEndTime = tEndTime;
	m_bPetEffect = m_bEffect;

	m_fBaseSpeedFactor = TDEF_SPEED;
	
	//Saddle

	m_fBaseSpeedFactor_org = m_fBaseSpeedFactor;

	m_fTDEFAULTSPEED = m_fBaseSpeedFactor;

	WORD m_wMonID = m_pPetTemp->m_wMonID;

	CTClientGame* pGame = CTClientGame::GetInstance();
	CTClientChar* pMyHost = pGame->FindPC(GetHostID());

	if(pMyHost && pMyHost == pGame->GetMainChar())
	{
		CTPetManageDlg* pDlg = static_cast<CTPetManageDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_PET_MANAGE));

		if(!pDlg->GetPetSaddle())
		{
			SetSaddle(176);
			m_wMonID = m_pPetTemp->m_wMonID;
		}
		else
		{
			SetSaddle(200);
			m_wMonID = m_pPetTemp->m_wSaddleMonID;
		}
	}

	m_pTEMP = CTChart::FindTMONTEMP( m_wMonID );
}

void CTClientPet::SetEffect(BYTE m_bEffect)
{
	m_bPetEffect = m_bEffect;
}
void CTClientPet::SetSaddle(WORD wSaddle)
{
	m_fBaseSpeedFactor = TDEF_SPEED;
	m_fBaseSpeedFactor = wSaddle/100.0f;	

	m_fBaseSpeedFactor_org = m_fBaseSpeedFactor;

	m_fTDEFAULTSPEED = m_fBaseSpeedFactor;
}
void CTClientPet::ApplyPEffect(CTClientRecall* pRecall, BYTE m_bEffect)
{
	if(m_bEffect > 0)
	{
		LPTITEMGRADEVISUAL pGradeVISUAL = this->GetPETVISUAL(m_bEffect);
		
		if(  pGradeVISUAL->m_pSkinTex && pGradeVISUAL)
		{
			LPTEXTURESET pSkinTEX = pGradeVISUAL->m_pSkinTex;
			BYTE bPSTYPE = pGradeVISUAL->m_bPSTYPE;

			MAPOBJPART::iterator itr,end;
			itr = pRecall->m_OBJ.m_mapDRAW.begin();
			end = pRecall->m_OBJ.m_mapDRAW.end();

			for(; itr!=end; ++itr)
			{
				VECTOROBJPART* pDRAW = itr->second;

				for( int i=0; i<INT(pDRAW->size()); ++i)
				{
					LPOBJPART pPART = (*pDRAW)[i];
					pPART->m_pTEX = CTClientObjBase::NewGradeObjTex(
						pPART->m_pTEX,
						TT_TEX,
						0,
						pSkinTEX,
						bPSTYPE);

					pPART->m_pTEX->m_pTEX[1]->m_dwCurTick = 0;
				}
			}
		}
	}
}

LPTITEMGRADEVISUAL CTClientPet::GetPETVISUAL(BYTE m_bEffect)
{
	WORD wGradeSkin = 0;
	switch( m_bEffect)
	{
	case IE_SEA:
		wGradeSkin = 4;
		break;
	case IE_FIRE:
		wGradeSkin = 5;
		break;
	case IE_LIGHTING:
		wGradeSkin = 6;
		break;
	case IE_ICE:
		wGradeSkin = 7;
		break;
	case IE_BLACK:
		wGradeSkin = 8;
		break;
	case IE_PINK:
		wGradeSkin = 9;
		break;
	case IE_STORM:
		wGradeSkin = 10;
		break;
	case IE_NATURE:
		wGradeSkin = 11;
		break;
	case IE_MAGMA:
		wGradeSkin = 12;
		break;
	case IE_NEON_GREEN:
		wGradeSkin = 30;
		break;
	case IE_MAGIC_ICE:
		wGradeSkin = 31;
		break;
	case IE_MAGIC_BLUE:
		wGradeSkin = 32;
		break;
	case IE_SHINY_PINK:
		wGradeSkin = 33;
		break;
	case IE_MAGIC_PINK:
		wGradeSkin = 34;
		break;
	case IE_ABYSS:
		wGradeSkin = 35;
		break;
	case IE_GOLD:
		wGradeSkin = 3;
		break;
	case IE_QT_PINK:
		wGradeSkin = 36;
		break;
	case IE_E1:
		wGradeSkin = 37;
		break;
	case IE_E3:
		wGradeSkin = 39;
		break;
	case IE_E4:
		wGradeSkin = 40;
		break;
	case IE_E5:
		wGradeSkin = 41;
		break;
	case IE_E6:
		wGradeSkin = 42;
		break;
	case IE_E7:
		wGradeSkin = 43;
		break;
	case IE_E8:
		wGradeSkin = 44;
		break;
	case IE_E9:
		wGradeSkin = 45;
		break;
	case IE_E10:
		wGradeSkin = 46;
		break;
	case IE_E11:
		wGradeSkin = 47;
		break;
	case IE_E12:
		wGradeSkin = 48;
		break;
	case IE_E14:
		wGradeSkin = 50;
		break;
	case IE_E15:
		wGradeSkin = 51;
		break;
	case IE_E16:
		wGradeSkin = 52;
		break;
	case IE_E17:
		wGradeSkin = 53;
		break;
	case IE_E18:
		wGradeSkin = 54;
		break;
	case IE_E19:
		wGradeSkin = 55;
		break;
	case IE_E20:
		wGradeSkin = 56;
		break;
	case IE_E21:
		wGradeSkin = 57;
		break;
	case IE_E22:
		wGradeSkin = 58;
		break;
	default:
		wGradeSkin = 0;
			break;
	}
		
	TITEMGRADEVISUALKEY Key;
	Key.m_wGrade = wGradeSkin;
	Key.m_bKind = 2;

	MAPTITEMGRADEVISUAL::iterator itr = CTChart::m_mapTITEMGRADEVISUAL.find(Key);
	if( itr != CTChart::m_mapTITEMGRADEVISUAL.end())
		return &(itr->second);
	

	return NULL;
}
// =======================================================
BOOL CTClientPet::TakeUp( CD3DDevice *pDevice,
						  CTachyonRes *pRES,
						  CTClientChar *pChar,
						  DWORD dwPivot)
{



	if( m_pTakeUpChar )
		return FALSE;

	MAPDWORD::iterator itr = m_OBJ.m_pOBJ->m_mapPIVOT.find(dwPivot);
	if( itr == m_OBJ.m_pOBJ->m_mapPIVOT.end() )
		return FALSE;

	if (pChar->m_bIsStuned || pChar->m_bIsHolded || !pChar->m_bCanSelected)
		return FALSE;

	ApplyPEffect(this,  m_bPetEffect);
	TACTION act = pChar->FindActionID(
		TA_RIDING,
		pChar->GetWeaponID(pChar->m_bMode));
	BYTE bHideOnCapeMode = CTChart::GetTACTION(TA_RIDING)->m_bHideOnCapeMode;

	if( pChar->m_bHideOnCapeMode != bHideOnCapeMode ||
		pChar->m_bEquipMode != act.m_bEquipMode )
	{
		pChar->m_bHideOnCapeMode = bHideOnCapeMode;
		pChar->m_bEquipMode = act.m_bEquipMode;

		pChar->ResetEQUIP(
			pDevice,
			pRES);
	}

	pChar->m_bAction = TA_RIDING;
	pChar->SetAction(
		act.m_dwActID,
		act.m_dwAniID);

	m_dwTakeUpPivot = itr->second + 1;
	pChar->SetRidingPet(this);
	m_nDIR = INT(pChar->m_wDIR) - INT(m_wDIR);

	m_mapEQUIP.insert( std::make_pair(m_dwTakeUpPivot,pChar) );
	D3DXMatrixIdentity(&pChar->m_vWorld);
	m_pTakeUpChar = pChar;

	m_bMouseDIR = m_pTakeUpChar->m_bMouseDIR;
	m_bKeyDIR = TKDIR_N; //m_pTakeUpChar->m_bKeyDIR;
	
	m_pTakeUpChar->m_vPushDIR = D3DXVECTOR2(0,0);
	m_pTakeUpChar->m_vJumpDIR = D3DXVECTOR2(0,0);
	m_pTakeUpChar->m_vFallDIR = D3DXVECTOR2(0,0);

	m_pTakeUpChar->m_dwPushTick = 0;
	m_pTakeUpChar->m_dwJumpTick = 0;
	m_pTakeUpChar->m_dwFallTick = 0;

	m_bCanSelected = TRUE;

	return TRUE;
}
// -------------------------------------------------------
CTClientChar* CTClientPet::TakeDown()
{
	if( !m_pTakeUpChar )
		return NULL;

	CTClientChar* pRet = m_pTakeUpChar;

	MAPOBJECT::iterator itr = m_mapEQUIP.find(m_dwTakeUpPivot);
	if( itr != m_mapEQUIP.end() )
		m_mapEQUIP.erase(itr);

	m_pTakeUpChar->m_wDIR = m_wDIR;
	m_pTakeUpChar->m_wMoveDIR = m_bKeyDIR; //MOVE_NONE
	m_pTakeUpChar->m_wPITCH = m_wPITCH;
	m_pTakeUpChar->m_nDIR = m_nDIR;
	m_pTakeUpChar->m_nPITCH = m_nPITCH;
	m_pTakeUpChar->m_bMouseDIR = m_bMouseDIR;
	m_pTakeUpChar->m_bKeyDIR = m_bKeyDIR;
	m_pTakeUpChar->m_vWorld = m_vWorld;
	m_pTakeUpChar->SetPosition(m_vPosition);

	m_vPushDIR = D3DXVECTOR2(0,0);
	m_vJumpDIR = D3DXVECTOR2(0,0);
	m_vFallDIR = D3DXVECTOR2(0,0);
	m_dwPushTick = 0;
	m_dwJumpTick = 0;
	m_dwFallTick = 0;
	m_nPITCH = 0;
	m_nDIR = 0;
	m_bMouseDIR = TKDIR_N;
	m_bKeyDIR = TKDIR_N;
	SetTAction(TA_STAND);
	//m_dwHP = 0;

	m_dwTakeUpPivot = 0;
	m_pTakeUpChar->SetRidingPet(NULL);
	m_pTakeUpChar = NULL;
	m_bDrawTalk = FALSE;
	m_bCanSelected = FALSE;
	
	SetSpeedWhenRiding(1);
	CTClientGame::GetInstance()->DeleteRecall(m_dwID, TRUE);

	return pRet;
}
// =======================================================

// =======================================================
BYTE CTClientPet::GetDrawName()
{

	if(	!CTClientGame::GetPcNAMEOption() /*||
		m_pTakeUpChar == CTClientGame::GetInstance()->GetMainChar()*/ )
	{
		return FALSE;
	}

	return TRUE;
}
// -------------------------------------------------------
CString CTClientPet::GetTitle()
{
	if( m_pTakeUpChar )
		return m_pTakeUpChar->GetTitle();
	else
		return CString("");
}
// -------------------------------------------------------
CString CTClientPet::GetName()
{
	if( m_pTakeUpChar )
		return m_pTakeUpChar->GetName();
	else
		return m_strPetName;
}
CString CTClientPet::GetUserTitle()
{
	if( m_pTakeUpChar )
		return m_pTakeUpChar->GetUserTitle();
	else
		return CString("");
}
void CTClientPet::CalcFrame(BOOL bUpdate)
{
    if( m_pTakeUpChar ) 
		m_pTakeUpChar->CalcFrame( bUpdate );

	CTClientRecall::CalcFrame( bUpdate );
}
// =======================================================
void CTClientPet::CalcHeight(LPD3DXVECTOR3 pPREV, CTClientMAP *pMAP, DWORD dwTick)
{
	CTClientRecall::CalcHeight(pPREV,pMAP,dwTick);

	if( m_pTakeUpChar )
	{
		m_pTakeUpChar->m_dwDropDamage = m_dwDropDamage;
	}
	else
	{
		if( m_bAction == TA_DEAD )
		{
			m_bAction = TA_DIE;
			m_bACTLevel = CTChart::GetTACTION( TA_DIE )->m_bLevel;
		}
	}

	m_dwDropDamage = 0;
}
// -------------------------------------------------------
void CTClientPet::Render(CD3DDevice *pDevice, CD3DCamera *pCamera)
{
	
	CTClientGame* pGame = CTClientGame::GetInstance();
	CTClientChar* pMyHost = pGame->FindPC(GetHostID());

	if(pMyHost)
	{
		m_bRun = pMyHost->m_bRun;
		//m_bHide = pMyHost->m_bHide;
	}

	if( m_pTakeUpChar )
		 m_fSpeedFactor = GetSpeedWhenRiding();


	//CTClientRecall::Render(pDevice,pCamera);







	CTClientMoveObj::Render(pDevice,pCamera);
}
// =======================================================
FLOAT CTClientPet::GetSpeedWhenRiding()
{
	/*CTClientGame* pGAME = CTClientGame::GetInstance();
	CTClientChar* pRIDER = pGAME->FindPC( m_dwHostID );
	CTPetManageDlg* pDlg = const_cast<CTPetManageDlg*>( pGAME->GetFrame( TFRAME_PET_MANAGE ) );

	if( pRIDER == pGAME->GetMainChar() )
		return pDlg->GetSaddleSpeed( pDlg->GetPetSaddle() ) / 100.0f;

	return m_fBaseSpeedFactor; //m_fBaseSpeedFactor == 0 ? pDlg->GetSaddleSpeed(pDlg->GetPetSaddle()) / 100.0f : m_fBaseSpeedFactor;*/
	if (!m_pTakeUpChar)
		return TDEF_SPEED;

	if( m_fBaseSpeedFactor < m_pTakeUpChar->m_fSpeedFactor )
		return m_pTakeUpChar->m_fSpeedFactor;
	else
		return m_fBaseSpeedFactor;



	
	m_fBaseSpeedFactor_org = m_fBaseSpeedFactor;
	return m_fBaseSpeedFactor * (m_pTakeUpChar->m_fSpeedFactor / m_pTakeUpChar->m_fTDEFAULTSPEED);

}

void CTClientPet::SetSpeedWhenRiding(WORD wMulti)
{


	float whip;


	if(wMulti == 1 || wMulti == 0)
	{
		whip = 0;
		m_fBaseSpeedFactor = m_fBaseSpeedFactor - m_fBaseSpeedFactor_org;
		m_fBaseSpeedFactor_org = 0;




































	}
	else
	{


		whip = m_fBaseSpeedFactor;
		m_fBaseSpeedFactor_org = m_fBaseSpeedFactor * 0.5f;
		m_fBaseSpeedFactor = m_fBaseSpeedFactor * 1.5f;
		
	}	
		
		



		D3DXVECTOR3 vDIR( 0.0f, 0.0f, -((TDEF_SPEED * 10) + whip));
		D3DXMATRIX vROT;

		D3DXMatrixRotationY(
			&vROT,
			FLOAT(m_wMoveDIR) * D3DX_PI / 900.0f);

		CTMath::Transform(
			&vROT,
			&vDIR);

		m_vJumpDIR.x = vDIR.x;
		m_vJumpDIR.y = vDIR.z;



}

void CTClientPet::ShowSFX()
{
	if( m_pTakeUpChar )
		m_pTakeUpChar->ShowSFX();
	CTClientRecall::ShowSFX();
}

void CTClientPet::HideSFX()
{
	if( m_pTakeUpChar )
		m_pTakeUpChar->HideSFX();
	CTClientRecall::HideSFX();
}

BYTE CTClientPet::CheckFall( CTClientMAP *pMAP,
							 LPD3DXVECTOR2 pFallDIR)
{
	return m_pTakeUpChar ? CTClientMoveObj::CheckFall( pMAP, pFallDIR) : CTClientRecall::CheckFall( pMAP, pFallDIR);
}
