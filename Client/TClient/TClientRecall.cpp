#include "StdAfx.h"
#include "TClientGame.h"
#include "TPetManageDlg.h"
#include "TClientPet.h"
D3DXVECTOR2 CTClientRecall::m_vTPOS[TRECALL_MAX] = {
	D3DXVECTOR2(1.8f, 0.0f),
	D3DXVECTOR2(-1.8f, 0.0f),
	D3DXVECTOR2(1.8f, 1.8f),
	D3DXVECTOR2(0.0f, 1.8f),
	D3DXVECTOR2(-1.8f, 1.8f) };


CTClientRecall::CTClientRecall()
{
	m_bCollisionType = TCOLLISION_CYLINDER;
	m_bTPOS = TRECALL_MAX;
	m_bType = OT_RECALL;

	m_bDEAD = FALSE;
	m_bDIE = FALSE;

	m_bSubAI = TRECALLAI_COUNT;
	m_bAI = TRECALLAI_PASSIVE;

	m_pTDEFSKILL = NULL;
	m_pTCURSKILL = NULL;

	m_dwTargetID = 0;
	m_dwHostID = 0;
	m_bPetEffect = 0;

	m_dwEndLifeTick = 0;

	m_wCompanionID = 0;

	m_bRecallType = TRECALLTYPE_NONE;
	m_bTargetType = OT_NONE;

	m_nRecallRunAwayIndex = 0;
	m_vRecallRunAway.clear();
	m_vRecallRunAwayTarget = D3DXVECTOR3(0.0f, 0.0f, 0.0f);

	m_bActionLock = FALSE;
	m_bSubActionLock = FALSE;
}

CTClientRecall::~CTClientRecall()
{
}

void CTClientRecall::ReleaseData()
{
	CTClientMoveObj::ReleaseData();

	m_bContryID = TCONTRY_N;
	m_bTPOS = TRECALL_MAX;
	m_pTEMP = NULL;

	m_bPetEffect = 0;
	m_bDEAD = FALSE;
	m_bDIE = FALSE;

	m_bSubAI = TRECALLAI_COUNT;
	m_bAI = TRECALLAI_PASSIVE;

	m_pTDEFSKILL = NULL;
	m_pTCURSKILL = NULL;

	m_dwTargetID = 0;
	m_dwHostID = 0;

	m_dwEndLifeTick = 0;

	m_bTargetType = OT_NONE;
	m_dwTargetID = 0;

	m_nRecallRunAwayIndex = 0;
	m_vRecallRunAway.clear();
	m_vRecallRunAwayTarget = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
}

DWORD CTClientRecall::CalcJumpDamage()
{
	if (m_pTEMP && m_pTEMP->m_bCanFly)
		return 0;

	return CTClientMoveObj::CalcJumpDamage();
}

DWORD CTClientRecall::CalcFallDamage()
{
	if (m_pTEMP && m_pTEMP->m_bCanFly)
		return 0;

	return CTClientMoveObj::CalcFallDamage();
}

BYTE CTClientRecall::IsAlliance(CTClientObjBase *pTARGET)
{
	CTClientGame* pGame = CTClientGame::GetInstance();

	if (pGame->IsDuel())
	{
		CTClientObjBase *pHostTarget = NULL;
		if (pTARGET->m_bType == OT_PC)
			pHostTarget = pTARGET;
		else if (pTARGET->m_bType == OT_RECALL || pTARGET->m_bType == OT_SELF)
			pHostTarget = pGame->FindPC(pTARGET->GetHostID());

		if (pHostTarget)
		{
			CTClientChar* pMyHost = pGame->FindPC(GetHostID());
			if (pMyHost)
			{
				CTClientChar* pMainChar = pGame->GetMainChar();
				CTClientChar* pDuelTarg = pGame->GetDuelTarget();
				if ((pMainChar == pMyHost && pDuelTarg == pHostTarget) ||
					(pDuelTarg == pMyHost && pMainChar == pHostTarget))
				{
					return FALSE;
				}
			}
		}
	}

	if (pGame->m_bChallengeGame)
	{
		CTClientObjBase *pHostTarget = NULL;
		if (pTARGET->m_bType == OT_PC)
			pHostTarget = pTARGET;
		else if (pTARGET->m_bType == OT_RECALL || pTARGET->m_bType == OT_SELF)
			pHostTarget = pGame->FindPC(pTARGET->GetHostID());

		CTClientChar* pMyHost = pGame->FindPC(GetHostID());
		if (pHostTarget && pMyHost)
		{
			if (pMyHost->m_bChallengeTeam != TTOURNAMENT_TEAM_NONE &&
				pHostTarget->m_bChallengeTeam != TTOURNAMENT_TEAM_NONE &&
				pHostTarget->m_bChallengeTeam != pMyHost->m_bChallengeTeam)
			{
				return FALSE;
			}
		}

		return TRUE;
	}

	if (pGame->IsTournamentBattle() || pGame->IsTournamentWatching())
	{
		CTClientObjBase *pHostTarget = NULL;
		if (pTARGET->m_bType == OT_PC)
			pHostTarget = pTARGET;
		else if (pTARGET->m_bType == OT_RECALL || pTARGET->m_bType == OT_SELF)
			pHostTarget = pGame->FindPC(pTARGET->GetHostID());

		CTClientChar* pMyHost = pGame->FindPC(GetHostID());
		if (pHostTarget && pMyHost)
		{
			if (pMyHost->m_bTournamentTeam != TTOURNAMENT_TEAM_NONE &&
				pHostTarget->m_bTournamentTeam != TTOURNAMENT_TEAM_NONE &&
				pHostTarget->m_bTournamentTeam != pMyHost->m_bTournamentTeam)
			{
				return FALSE;
			}
		}

		return TRUE;
	}

	return CTClientMoveObj::IsAlliance(pTARGET);
}

CString CTClientRecall::GetTitle()
{
	CString strTITLE = m_pTEMP ? m_pTEMP->m_strTITLE : CString("");
	strTITLE.Remove('_');

	return strTITLE;
}

CString CTClientRecall::GetName()
{
	CString strNAME = m_pTEMP ? m_pTEMP->m_strNAME : CString("");
	strNAME.Remove('_');

	return strNAME;
}

void CTClientRecall::InitRecall(CD3DDevice *pDevice,
	CTachyonRes *pRES,
	WORD wTempID,
	BYTE bLevel)
{
	CTClientInven *pTEQUIP = new CTClientInven();
	ClearInven();
	pTEQUIP->m_bInvenID = INVEN_EQUIP;
	pTEQUIP->m_wItemID = 0;
	m_mapTINVEN.insert(MAPTINVEN::value_type(pTEQUIP->m_bInvenID, pTEQUIP));

	LPTMONTEMP pTMON = CTChart::FindTMONTEMP(wTempID);
	if (pTMON)
		m_pTEMP = pTMON;

	CTClientPet* pPet = static_cast<CTClientPet*>(this);
	ApplyPetEffect(this);
	if (m_pTEMP && m_pTEMP->m_dwOBJ)
	{
		InitOBJ(pRES->GetOBJ(m_pTEMP->m_dwOBJ));

		for (BYTE i = 0; i < ES_COUNT; i++)
			if (m_pTEMP->m_pTITEM[i])
			{
				CTClientItem *pTITEM = new CTClientItem();

				pTITEM->SetItemSlot(i);
				pTITEM->SetTITEM(m_pTEMP->m_pTITEM[i]);
				pTITEM->SetCount(1);

				pTEQUIP->m_mapTITEM.insert(make_pair(pTITEM->GetItemSlot(), pTITEM));
			}
		ResetEQUIP(pDevice, pRES);

		ResetOBJPart(pDevice);
		m_fBreathHeight = GetAttrFLOAT(ID_BREATH_HEIGHT) * m_pTEMP->m_fScaleY;
		m_bCanSelected = m_pTEMP->m_bCanSelected;
		m_bHide = !m_pTEMP->m_bVisible;
		m_bDrawName = m_pTEMP->m_bDrawName;

		InitSIZE(
			m_pTEMP->m_fSize,
			m_pTEMP->m_fScaleY * GetAttrFLOAT(ID_SIZE_Y),
			m_pTEMP->m_fSize,
			0.0f, 0.0f);

		D3DXMatrixScaling(
			&m_vScale,
			m_pTEMP->m_fScaleX,
			m_pTEMP->m_fScaleY,
			m_pTEMP->m_fScaleZ);

		m_vScaleSFX = CTMath::Inverse(&m_vScale);
		m_vPosition = m_vScale * m_vWorld;

		for (auto i = 0; i < TMONSKILL_COUNT; i++)
		{
			LPTSKILL pTSKILL = CTChart::FindTSKILLTEMP(m_pTEMP->m_wTSKILL[i]);

			if (pTSKILL)
			{
				CTClientSkill *pTSkill = new CTClientSkill();

				pTSkill->m_pTSKILL = pTSKILL;
				pTSkill->m_dwTick = 0;
				pTSkill->m_bLevel = min(bLevel, pTSKILL->m_bMaxLevel);

				m_mapTSKILL.insert(MAPTSKILL::value_type(m_pTEMP->m_wTSKILL[i], pTSkill));
			}
		}

		m_pTCURSKILL = FindTSkill(m_pTEMP->m_wTSKILL[TMONSKILL_DEFAULT]);
	}

	m_vTSKILLDATA.m_bAtkCountryID = m_bContryID;
	m_vTSKILLDATA.m_bAidAtkCountryID = m_bAidCountryID;
	m_vTSKILLDATA.m_bAglow = m_bCanSelected;

	switch (m_bRecallType)
	{
	case TRECALLTYPE_MAIN: m_bAI = TRECALLAI_PASSIVE; break;
	case TRECALLTYPE_MINE:
	case TRECALLTYPE_AUTOAI: m_bAI = TRECALLAI_ACTIVE; break;
	case TRECALLTYPE_PET: m_bCanDefend = FALSE;
	case TRECALLTYPE_MAINTAIN:
	case TRECALLTYPE_SKILL:
	case TRECALLTYPE_SPY: m_bAI = TRECALLAI_MANUAL; break;
	}

	if (wTempID >= 31250)
	{
		WORD wIndex = wTempID - 31250;
		switch (wIndex)
		{
		case 0:
		case 1:
		{
			ApplyCustomTexture(14, 1);
			ApplyCustomTexture(16, 2);
		}
		break;
		case 2:
		case 3:
		{
			ApplyCustomTexture(20, 1);
		}
		break;
		case 4:
		case 5:
		{
			ApplyCustomTexture(21, 1);
		}
		break;
		case 6:
		case 7:
		{
			ApplyCustomTexture(19, 1);
		}
		break;
		case 8:
		case 9:
		{
			ApplyCustomTexture(22, 1);
		}
		break;
		}
	}




}
void CTClientRecall::ApplyPetEffect(CTClientRecall* pPET)
{
	if (m_bPetEffect > 0)
	{
		LPTITEMGRADEVISUAL pGradeVISUAL = pPET->GetRECVISUAL();
		if (pGradeVISUAL && pGradeVISUAL->m_pSkinTex)
		{
			LPTEXTURESET pSkinTEX = pGradeVISUAL->m_pSkinTex;
			BYTE bPSTYPE = pGradeVISUAL->m_bPSTYPE;

			MAPOBJPART::iterator itr, end;
			itr = pPET->m_OBJ.m_mapDRAW.begin();
			end = pPET->m_OBJ.m_mapDRAW.end();

			for (; itr != end; ++itr)
			{
				VECTOROBJPART* pDRAW = itr->second;

				for (int i = 0; i < INT(pDRAW->size()); ++i)
				{
					LPOBJPART pPART = (*pDRAW)[i];
					pPART->m_pTEX = CTClientObjBase::NewGradeObjTex(
						pPART->m_pTEX,
						TT_TEX,
						0,
						pSkinTEX,
						bPSTYPE);
				}
			}
		}
	}
	return;

}
LPTITEMGRADEVISUAL CTClientRecall::GetRECVISUAL()
{

	WORD wGradeSkin = 0;


	switch (m_bPetEffect)
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
	}

	TITEMGRADEVISUALKEY Key;
	Key.m_wGrade = wGradeSkin;
	Key.m_bKind = 2;

	MAPTITEMGRADEVISUAL::iterator itr = CTChart::m_mapTITEMGRADEVISUAL.find(Key);
	if (itr != CTChart::m_mapTITEMGRADEVISUAL.end())
		return &(itr->second);


	return NULL;
}

BYTE CTClientRecall::GetTAction()
{
	return CTClientMoveObj::GetTAction();
}

CTClientSkill *CTClientRecall::GetBestTSKILL(CTClientObjBase *pTARGET)
{
	return NULL;
	FLOAT fDIST = pTARGET ? D3DXVec3LengthSq(&(pTARGET->GetPosition() - GetPosition())) : TCAM_LENGTH * TCAM_LENGTH;
	FLOAT fMAX = 0.0f;

	CTClientSkill *pTRESULT = NULL;
	MAPTSKILL::iterator itTSKILL;

	for (itTSKILL = m_mapTSKILL.begin(); itTSKILL != m_mapTSKILL.end(); itTSKILL++)
		if ((*itTSKILL).second->m_pTSKILL->m_bLoop)
		{
			FLOAT fLOCAL = GetMinRange(
				pTARGET,
				(*itTSKILL).second->m_pTSKILL);

			if (fLOCAL = 0.0f || fDIST > fLOCAL * fLOCAL)
			{
				fLOCAL = TMAXRANGE_RATIO * GetMaxRange(
					pTARGET,
					(*itTSKILL).second->m_pTSKILL);

				if (!pTRESULT || fMAX < fLOCAL)
				{
					pTRESULT = (*itTSKILL).second;
					fMAX = fLOCAL;
				}
			}
		}

	if (!pTRESULT)
		pTRESULT = m_pTCURSKILL;

	return pTRESULT;
}

D3DXVECTOR3 CTClientRecall::GetRoamTarget(LPD3DXMATRIX pDIR,
	FLOAT fPosX,
	FLOAT fPosY,
	FLOAT fPosZ)
{
	D3DXVECTOR3 vRESULT;

	if (m_bType != OT_SELF)
	{
		vRESULT = m_bTPOS < TRECALL_MAX ? D3DXVECTOR3(
			m_vTPOS[m_bTPOS].x,
			0.0f,
			m_vTPOS[m_bTPOS].y) : D3DXVECTOR3(
				0.0f,
				0.0f,
				0.0f);

		CTMath::Transform(
			pDIR,
			&vRESULT);

		vRESULT.x += fPosX;
		vRESULT.y += fPosY;
		vRESULT.z += fPosZ;
	}
	else
	{
		vRESULT.x = m_vPosition._41;
		vRESULT.y = m_vPosition._42;
		vRESULT.z = m_vPosition._43;
	}

	return vRESULT;
}

D3DXVECTOR3 CTClientRecall::AdjustRoamTarget(
	CTClientChar* pHOST,
	CTClientMAP* pMAP,
	LPD3DXMATRIX pDIR,
	D3DXVECTOR3 vTARGET)
{
	if (m_bType == OT_SELF || pHOST == NULL || pMAP == NULL)
		return vTARGET;

	D3DXVECTOR3 vHOST = pHOST->GetPosition();

	if (pMAP->CanMove(
		this,
		&vHOST,
		&vTARGET))
	{
		return vTARGET;
	}

	// 새로운 Pos를 찾는다.
	for (BYTE i = 1; i < TRECALL_MAX; ++i)
	{
		BYTE bTPOS = (m_bTPOS + i) % TRECALL_MAX;

		if (pHOST->m_vTRECALL[bTPOS])
			continue; // 이미 자리 있음.

		D3DXVECTOR3 vNewTarget(m_vTPOS[bTPOS].x, 0.0f, m_vTPOS[bTPOS].y);
		CTMath::Transform(
			pDIR,
			&vNewTarget);

		vNewTarget += vHOST;

		if (pMAP->CanMove(
			this,
			&vHOST,
			&vNewTarget))
		{
			pHOST->m_vTRECALL[m_bTPOS] = FALSE; // 위치 이동
			m_bTPOS = bTPOS;
			pHOST->m_vTRECALL[m_bTPOS] = TRUE;
			return vNewTarget;
		}
	}

	// 없으면 임시로 최단거리
	D3DXVECTOR2 vDir;
	vDir.x = m_vPosition._41 - vHOST.x;
	vDir.y = m_vPosition._43 - vHOST.z;

	D3DXVec2Normalize(&vDir, &vDir);
	vDir *= 1.8f;

	vTARGET.x = vHOST.x + vDir.x;
	vTARGET.y = vHOST.y;
	vTARGET.z = vHOST.z + vDir.y;
	return vTARGET;
}

BYTE CTClientRecall::GetRoamACT(LPD3DXVECTOR3 pTARGET)
{
	if (m_bType != OT_SELF)
	{
		FLOAT fDIST = D3DXVec2LengthSq(&D3DXVECTOR2(
			pTARGET->x - m_vPosition._41,
			pTARGET->z - m_vPosition._43));
		FLOAT fSB = TROAM_BOUND;

		if (m_bSubAI == TRECALLAI_STAY || fDIST < fSB * fSB)
			return TA_STAND;

		if (fDIST < TRECALL_WALK_BOUND * TRECALL_WALK_BOUND)
			return m_bAction == TA_STAND || m_bAction == TA_WALK ? TA_WALK : TA_RUN;
	}
	else
		return TA_STAND;

	return TA_RUN;
}

FLOAT CTClientRecall::GetLOST(CTClientObjBase *pTARGET)
{
	FLOAT fRESULT = m_pTEMP ? m_pTEMP->m_fLOST + m_fRadius : 0.0f;

	if (pTARGET)
		fRESULT += pTARGET->m_fRadius;

	return fRESULT;
}

FLOAT CTClientRecall::GetAB(CTClientObjBase *pTARGET)
{
	FLOAT fRESULT = m_pTEMP ? m_pTEMP->m_fAB + m_fRadius : 0.0f;

	if (pTARGET)
		fRESULT += pTARGET->m_fRadius;

	return fRESULT;
}

FLOAT CTClientRecall::GetLB(CTClientObjBase *pTARGET)
{
	FLOAT fRESULT = m_pTEMP ? m_pTEMP->m_fLB + m_fRadius : 0.0f;

	if (pTARGET)
		fRESULT += pTARGET->m_fRadius;

	return fRESULT;
}

DWORD CTClientRecall::GetHostID()
{
	return m_dwHostID;
}

DWORD CTClientRecall::GetLeftLifeTick()
{
	DWORD dwCurTick = ::GetTickCount();
	if (m_dwEndLifeTick <= dwCurTick)
		return 0;

	return m_dwEndLifeTick - dwCurTick;
}

void CTClientRecall::DoRETRANS(CD3DDevice *pDevice,
	CTachyonRes *pRES)
{
	CTClientMoveObj::DoRETRANS(pDevice, pRES);

	InitOBJ(pRES->GetOBJ(m_pTEMP->m_dwOBJ));
	ResetEQUIP(pDevice, pRES);

	m_fBreathHeight = GetAttrFLOAT(ID_BREATH_HEIGHT);
	m_bCanSelected = m_pTEMP->m_bCanSelected;

	InitSIZE(
		m_pTEMP->m_fSize,
		m_pTEMP->m_fScaleY * GetAttrFLOAT(ID_SIZE_Y),
		m_pTEMP->m_fSize,
		0.0f, 0.0f);

	ResetRootID(ID_PIVOT_WAIST);
}

LRESULT CTClientRecall::OnActEndMsg()
{
	if (IsDead())
		m_bDEAD = TRUE;

	return CTClientMoveObj::OnActEndMsg();
}

void CTClientRecall::OnShotSkill()
{
	//if( m_bRecallType == TRECALLTYPE_MINE )
	//	m_bDIE = TRUE;
}

BYTE CTClientRecall::GetDrawName()
{
	return CTClientObjBase::GetDrawName() && CTClientGame::GetMonNAMEOption() ? TRUE : FALSE;
}

BYTE CTClientRecall::CanDIVE()
{
	return m_pTEMP && m_pTEMP->m_bCanFly ? FALSE : TRUE;
}

BYTE CTClientRecall::CheckFall(CTClientMAP *pMAP,
	LPD3DXVECTOR2 pFallDIR)
{
	return FALSE;
}

BYTE CTClientRecall::Fall(LPD3DXVECTOR2 pFallDIR)
{
	if (IsRide() || IsFall())
		return FALSE;

	if (CTChart::GetTACTION(TA_FALL)->m_bLevel >= m_bACTLevel)
	{
		TACTION vActionID = FindActionID(
			TA_STAND,
			GetWeaponID(m_bMode));
		SetTAction(TA_FALL);

		m_bACTLevel = CTChart::GetTACTION(TA_FALL)->m_bLevel;
		SetAction(vActionID.m_dwActID, vActionID.m_dwAniID);
	}

	if (pFallDIR->x != 0.0f || pFallDIR->y != 0.0f)
		(*pFallDIR) /= D3DXVec2Length(pFallDIR);

	m_fFallHeight = m_vPosition._42;
	m_dwFallTick = 0;
	m_vFallDIR = max(TMINFALL_SPEED, m_fMoveSpeed) * (*pFallDIR);

	return TRUE;
}

void CTClientRecall::ResetDefaultSkill()
{
	if (m_pTEMP)
	{
		m_pTCURSKILL = FindTSkill(m_pTEMP->m_wTSKILL[TMONSKILL_DEFAULT]);
		m_pTCURSKILL->m_bTimerON = FALSE;
		m_pTCURSKILL->m_dwTick = 0;
		m_pTCURSKILL->m_dwExceptTick = 0;
	}

	m_bTargetType = OT_NONE;
	m_dwTargetID = 0;
	m_bSubAI = TRECALLAI_COUNT;

	switch (m_bRecallType)
	{
	case TRECALLTYPE_MINE:
	case TRECALLTYPE_AUTOAI: m_bAI = TRECALLAI_ACTIVE; break;
	case TRECALLTYPE_PET: m_bCanDefend = FALSE;
	case TRECALLTYPE_MAINTAIN:
	case TRECALLTYPE_SKILL:
	case TRECALLTYPE_SPY: m_bAI = TRECALLAI_MANUAL; break;
	}
}
void CTClientRecall::Render(CD3DDevice *pDevice, CD3DCamera *pCamera)
{
	CTClientGame* pGame = CTClientGame::GetInstance();
	CTClientChar* pMyHost = pGame->FindPC(GetHostID());

	if (pMyHost)
	{
		m_bRun = pMyHost->m_bRun;
		//m_bHide = pMyHost->m_bHide;

		if (pMyHost->GetRidingPet())
			m_fSpeedFactor = pMyHost->GetRidingPet()->m_fSpeedFactor;
		//	else
		//		m_fSpeedFactor = pMyHost->m_fSpeedFactor;
	}


	CTClientMoveObj::Render(pDevice, pCamera);
}