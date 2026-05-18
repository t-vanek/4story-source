#include "StdAfx.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "TPetManageDlg.h"

D3DXVECTOR2 CTClientSpolecnik::m_vTPOS[TRECALL_MAX] = {
	D3DXVECTOR2(1.8f, 0.0f),
	D3DXVECTOR2(-1.8f, 0.0f),
	D3DXVECTOR2(1.8f, 1.8f),
	D3DXVECTOR2(0.0f, 1.8f),
	D3DXVECTOR2(-1.8f, 1.8f) };


CTClientSpolecnik::CTClientSpolecnik()
{
	m_bCollisionType = TCOLLISION_CYLINDER;
	m_bTPOS = TRECALL_MAX;
	m_bType = OT_COMPANION;

	m_bDEAD = FALSE;
	m_bDIE = FALSE;

	m_bSubAI = TRECALLAI_BACK;
	m_bAI = TRECALLAI_BACK;

	m_dwTargetID = 0;
	m_dwHostID = 0;


	m_bSpolecnikType = TRECALLTYPE_NONE;
	m_bTargetType = OT_NONE;

	m_nSpolecnikRunAwayIndex = 0;
	m_vSpolecnikRunAway.clear();
	m_vSpolecnikRunAwayTarget = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	m_fTDEFAULTSPEED = TDEF_SPEED;
	m_bCanSelected = FALSE;
	m_bTrans = FALSE;
	m_bWalking = FALSE;

	m_bEffect = 0;

	m_bCanTeleport = TRUE;

}

CTClientSpolecnik::~CTClientSpolecnik()
{
}

void CTClientSpolecnik::ReleaseData()
{
	CTClientMoveObj::ReleaseData();

	m_bContryID = TCONTRY_N;
	m_bTPOS = TRECALL_MAX;
	m_pTEMP = NULL;

	m_bDEAD = FALSE;
	m_bDIE = FALSE;

	m_bSubAI = TRECALLAI_COUNT;
	m_bAI = TRECALLAI_PASSIVE;


	m_dwTargetID = 0;
	m_dwHostID = 0;

	m_bTargetType = OT_NONE;
	m_dwTargetID = 0;

	m_nSpolecnikRunAwayIndex = 0;
	m_vSpolecnikRunAway.clear();
	m_vSpolecnikRunAwayTarget = D3DXVECTOR3(0.0f, 0.0f, 0.0f);

	m_fTDEFAULTSPEED = TDEF_SPEED;
	m_bCanSelected = FALSE;


	m_bWalking = FALSE;
	m_bEffect = 0;
	m_bTrans = FALSE;
	m_bCanTeleport = TRUE;
}

DWORD CTClientSpolecnik::CalcJumpDamage()
{
	if (m_pTEMP && m_pTEMP->m_bCanFly)
		return 0;

	return CTClientMoveObj::CalcJumpDamage();
}

DWORD CTClientSpolecnik::CalcFallDamage()
{
	if (m_pTEMP && m_pTEMP->m_bCanFly)
		return 0;

	return CTClientMoveObj::CalcFallDamage();
}

BYTE CTClientSpolecnik::IsAlliance(CTClientObjBase *pTARGET)
{
	CTClientGame* pGame = CTClientGame::GetInstance();

	if (pGame->IsDuel())
	{
		CTClientObjBase *pHostTarget = NULL;
		if (pTARGET->m_bType == OT_PC)
			pHostTarget = pTARGET;
		else if (pTARGET->m_bType == OT_COMPANION || pTARGET->m_bType == OT_SELF)
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
		else if (pTARGET->m_bType == OT_COMPANION || pTARGET->m_bType == OT_SELF)
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
		else if (pTARGET->m_bType == OT_COMPANION || pTARGET->m_bType == OT_SELF)
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

CString CTClientSpolecnik::GetTitle()
{
	return "";
}

CString CTClientSpolecnik::GetName()
{
	return "";
}

void CTClientSpolecnik::InitSpolecnik(CD3DDevice *pDevice,
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

		m_fBreathHeight = GetAttrFLOAT(ID_BREATH_HEIGHT) * m_pTEMP->m_fScaleY;
		m_bCanSelected = FALSE;
		m_bHide = !m_pTEMP->m_bVisible;
		m_bDrawName = m_pTEMP->m_bDrawName;

		if (wTempID != PETBALL_MONID) //is he like somehow stupid or something
		{
			FLOAT fMax = m_pTEMP->m_fPetSize;
			if (bLevel > MAX_PET_LEVEL)
				bLevel = MAX_PET_LEVEL;

			FLOAT fMain = fMax / (GetAttrFLOAT(ID_SIZE_Y) * fMax) / MAX_PET_LEVEL;
			fMain *= bLevel + 8;

			FLOAT fCustomSize = CTClientGame::GetInstance()->FindCustomPetSize(wTempID);
			if (fCustomSize != -1.0f)
				fMain *= fCustomSize;

			D3DXMatrixScaling(
				&m_vScale,
				fMain,
				fMain,
				fMain);

			InitSIZE(
				fMain,
				m_pTEMP->m_fPetSize,
				fMain,
				0.0f, 0.0f);

			m_vScaleSFX = CTMath::Inverse(&m_vScale);
			m_vPosition = m_vScale * m_vWorld;

			//	DrawEffect();

		}
		else
		{
			FLOAT fMain = 1.5f;
			BYTE bLevel = m_bLevel;

			if (bLevel > MAX_PET_LEVEL)
				bLevel = MAX_PET_LEVEL;

			fMain += bLevel * 0.1f;

			D3DXMatrixScaling(
				&m_vScale,
				fMain,
				fMain,
				fMain);
			InitSIZE(
				0.5f,
				m_pTEMP->m_fScaleY * GetAttrFLOAT(ID_SIZE_Y),
				0.5f,
				0.0f, 0.0f);

			m_vScaleSFX = CTMath::Inverse(&m_vScale);
			m_vPosition = m_vScale * m_vWorld;



			GenerateSFX(
				&m_mapOBJSFXINST,

				&m_vTGARBAGESFX,

				&m_mapOBJSFX,
				0.0f,

				GetPetBallID());

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

	DrawEffect();

	m_vTSKILLDATA.m_bAtkCountryID = m_bContryID;
	m_vTSKILLDATA.m_bAidAtkCountryID = m_bAidCountryID;
	m_vTSKILLDATA.m_bAglow = m_bCanSelected;


}

BYTE CTClientSpolecnik::GetTAction()
{

	return CTClientMoveObj::GetTAction();
}


D3DXVECTOR3 CTClientSpolecnik::GetRoamTarget(LPD3DXMATRIX pDIR,
	FLOAT fPosX,
	FLOAT fPosY,
	FLOAT fPosZ)
{
	D3DXVECTOR3 vRESULT;

	vRESULT = m_bTPOS < TRECALL_MAX ? D3DXVECTOR3(
		0.0f,//m_vTPOS[m_bTPOS].x,//x
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



	return vRESULT;
}

D3DXVECTOR3 CTClientSpolecnik::AdjustRoamTarget(
	CTClientChar* pHOST,
	CTClientMAP* pMAP,
	LPD3DXMATRIX pDIR,
	D3DXVECTOR3 vTARGET)
{
	if (pHOST == NULL || pMAP == NULL)
		return vTARGET;

	D3DXVECTOR3 vHOST = pHOST->GetPosition();

	if (pMAP->CanMove(
		this,
		&vHOST,
		&vTARGET))
	{
		return vTARGET;
	}

	// »o·Îzî Pos¸¦ AL´Â´U.
	for (BYTE i = 1; i < TRECALL_MAX; ++i)
	{
		BYTE bTPOS = (m_bTPOS + i) % TRECALL_MAX;

		if (pHOST->m_vTSPOLECNIK[bTPOS])
			continue; // REaE RÚ¸® RÖR?.

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
			pHOST->m_vTSPOLECNIK[m_bTPOS] = FALSE; // R§Ä? REµz
			m_bTPOS = bTPOS;
			pHOST->m_vTSPOLECNIK[m_bTPOS] = TRUE;
			return vNewTarget;
		}
	}

	// lrR¸¸é RÓ?A·Î AÖ´Ü°L¸®
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

BYTE CTClientSpolecnik::GetRoamACT(LPD3DXVECTOR3 pTARGET)
{
	CTClientGame* pGame = CTClientGame::GetInstance();
	FLOAT fDIST = D3DXVec2LengthSq(&D3DXVECTOR2(
		pTARGET->x - m_vPosition._41,
		pTARGET->z - m_vPosition._43));

	FLOAT fSB = 2.5f;

	if (m_bTrans)
		fSB = 0.8f;

	if (fDIST < fSB * fSB)
		return TA_STAND;

	FLOAT fFB = 3.5f;

	if (!m_bTrans)
		if (fDIST < fFB * fFB)
		{
			m_bWalking = m_bAction == TA_STAND ? TRUE : FALSE;
			return m_bAction == TA_STAND || m_bWalking ? TA_STAND : TA_RUN;
		}

	m_bWalking = FALSE;

	return TA_RUN;
}

FLOAT CTClientSpolecnik::GetLOST(CTClientObjBase *pTARGET)
{
	FLOAT fRESULT = m_pTEMP ? m_pTEMP->m_fLOST + m_fRadius : 0.0f;

	if (pTARGET)
		fRESULT += pTARGET->m_fRadius;

	return fRESULT;
}

FLOAT CTClientSpolecnik::GetAB(CTClientObjBase *pTARGET)
{
	FLOAT fRESULT = m_pTEMP ? m_pTEMP->m_fAB + m_fRadius : 0.0f;

	if (pTARGET)
		fRESULT += pTARGET->m_fRadius;

	return fRESULT;
}

FLOAT CTClientSpolecnik::GetLB(CTClientObjBase *pTARGET)
{
	FLOAT fRESULT = m_pTEMP ? m_pTEMP->m_fLB + m_fRadius : 0.0f;

	if (pTARGET)
		fRESULT += pTARGET->m_fRadius;

	return fRESULT;
}

DWORD CTClientSpolecnik::GetHostID()
{
	return m_dwHostID;
}



LRESULT CTClientSpolecnik::OnActEndMsg()
{
	if (IsDead())
		m_bDEAD = TRUE;

	return CTClientMoveObj::OnActEndMsg();
}



BYTE CTClientSpolecnik::GetDrawName()
{
	return CTClientObjBase::GetDrawName() && CTClientGame::GetMonNAMEOption() ? TRUE : FALSE;
}

BYTE CTClientSpolecnik::CanDIVE()
{
	return m_pTEMP && m_pTEMP->m_bCanFly ? FALSE : TRUE;
}

BYTE CTClientSpolecnik::CheckFall(CTClientMAP *pMAP,
	LPD3DXVECTOR2 pFallDIR)
{
	return FALSE;
}

BYTE CTClientSpolecnik::Fall(LPD3DXVECTOR2 pFallDIR)
{
	if (m_bTrans)
		return FALSE;
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


BYTE CTClientSpolecnik::GetSlot()
{
	return bSlot;
}

void CTClientSpolecnik::Render(CD3DDevice *pDevice, CD3DCamera *pCamera)
{
	CTClientGame* pGAME = CTClientGame::GetInstance();
	CTClientChar* pHOST = pGAME->FindPC(GetHostID());
	CTachyonRes* pRES = pGAME->GetResource();





	if (pHOST)




	{
		m_fSpeedFactor = pHOST->m_fSpeedFactor;















		CTClientPet * pPET = pHOST->GetRidingPet();
		if (pPET)
			m_fSpeedFactor = pPET->m_fSpeedFactor;


		D3DXVECTOR3 vTARGET = pHOST->GetPosition();
		D3DXVECTOR3 vTPOS = GetPosition();

		FLOAT fDIST = D3DXVec2LengthSq(&D3DXVECTOR2(
			vTARGET.x - vTPOS.x,
			vTARGET.z - vTPOS.z));

		if (m_bTrans)
			m_fSpeedFactor = fDIST / 4.0f; //THIS IS WHAT I MADE AND THIS IS OKAY.

		if (!m_bTrans && m_bWalking)
			m_fSpeedFactor *= 0.8f;

		if (!m_bTrans)
		{


			if (pHOST->GetRidingPet() ||
				pHOST->m_bSwim ||
				pHOST->m_bInLava ||
				pHOST->m_bDive ||
				pHOST->m_bMode == MT_BATTLE)
			{
				DoTRANS(pDevice, pRES, PETBALL_MONID);

				m_bTrans = TRUE;

			}
		}
		else
		{

			if (!(pHOST->GetRidingPet() ||
				pHOST->m_bSwim ||
				pHOST->m_bInLava ||
				pHOST->m_bDive ||
				pHOST->m_bMode == MT_BATTLE))
			{
				DoRETRANS(pDevice, pRES);
				m_bTrans = FALSE;

			}

		}

		// ˇ GOOD CODE STARTS RIGHT THERE ˇ
		if (m_bTrans &&
			(!m_vOBJSFX.size() ||
				!m_mapOBJSFX.size())) //petball fix
		{
			GenerateSFX(
				&m_mapOBJSFXINST,
				&m_vOBJSFX,
				&m_mapOBJSFX,
				0.0f,
				GetPetBallID());
		}

		if (pHOST == pGAME->GetMainChar())
		{
			FLOAT fTotalDist = D3DXVec3LengthSq(&D3DXVECTOR3(
				pHOST->GetPositionX() - m_vWorld._41,
				pHOST->GetPositionY() - m_vWorld._42,
				pHOST->GetPositionZ() - m_vWorld._43));

			if (abs(fTotalDist) > MAX_PET_DIST)
				if (pGAME->GetSession() &&
					m_bCanTeleport &&
					m_vWorld._42 > 7.0f)
				{
					pGAME->GetSession()->SendCS_HIDECOMPANION_REQ();
					m_bCanTeleport = FALSE;
				}
		}

	}

	CTClientObjBase::Render(pDevice, pCamera);

}

void CTClientSpolecnik::CalcHeight(LPD3DXVECTOR3 pPREV,
	CTClientMAP *pMAP,
	DWORD dwTick)
{
	if (!pMAP)
		return;

	if (m_bTrans)
	{
		CTClientGame *pTGAME = CTClientGame::GetInstance();
		CTClientChar* pTARGET = pTGAME->FindPC(m_dwHostID);

		D3DXVECTOR2 vMOVE = D3DXVECTOR2(
			m_vPosition._41 - pPREV->x,
			m_vPosition._43 - pPREV->z);

		CTClientObjBase *pFLOOR = NULL;

		FLOAT fMove = m_fMoveSpeed * FLOAT(dwTick) / 1000.0f;
		FLOAT fRevH = 0.0f;

		BYTE bFALL = FALSE;
		BYTE bLAND = FALSE;

		FLOAT fWaterHeight = TMIN_HEIGHT;
		FLOAT fLavaHeight = TMIN_HEIGHT;
		FLOAT fHeight = TMIN_HEIGHT;

		if (ForecastHeight(pMAP, &fHeight, &fWaterHeight, &fLavaHeight, dwTick))
		{
			m_vWorld._42 += fHeight - m_vPosition._42;
			m_vPosition._42 = fHeight;
		}
		else if (IsPush() || IsFall() || IsJump())
			bLAND = FALSE;

		fHeight = pMAP->GetHeight(
			this, &pFLOOR,
			&D3DXVECTOR3(
				m_vPosition._41,
				max(pPREV->y, m_vPosition._42) + 0.2f,
				m_vPosition._43),
			fMove,
			TRUE);

		if (pFLOOR)
		{
			m_bHouseMesh = pFLOOR->m_bHouseMesh;
			m_dwHouseID = pFLOOR->m_dwHouseID;
			m_bLand = FALSE;
		}
		else
		{
			m_bHouseMesh = FALSE;
			m_dwHouseID = 0;
			m_bLand = TRUE;
		}

		m_fSquareHeight = fHeight;
		m_bSwim = FALSE;

		if (m_dwANISNDFuncID && m_bSTEPSND)
		{
			if (fWaterHeight + m_fBreathHeight > fHeight)
				m_dwANISNDFuncID = TWATERSTEP_SND;
			else if (m_bLand)
				m_dwANISNDFuncID = pMAP->GetTStepSND(m_vPosition._41, m_vPosition._42, m_vPosition._43);
			else
				m_dwANISNDFuncID = TDEFSTEP_SND;

			m_bSTEPSND = FALSE;
		}

		if (pTARGET)
		{
			D3DXVECTOR3 vTPOS = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
			D3DXVECTOR3 vTPOSChar = pTARGET->GetPosition();

			if (!pTARGET->IsRide())
				vTPOS.y += pTARGET->GetAttrFLOAT(23649);
			else
			{
				CTClientObjBase* pMount = pTARGET->m_pRidingPet; // THIS IS NOT WHAT I MADE AND THIS IS NOT OKAY
				if (pMount)
				{
					vTPOS.y += pMount->GetAttrFLOAT(24187);
					vTPOS.y += 0.55f;
				}

			}

			fHeight = vTPOSChar.y;
			fHeight += vTPOS.y;
			if (CanFLY())
			{
				FLOAT fRozdil = (vTPOS.y / 20);
				FLOAT fMinSKY = max(fHeight, fWaterHeight);
				FLOAT fMaxSKY = fHeight + fRozdil;

				fHeight = max(fMinSKY, m_vPosition._42);
				fHeight = min(fMaxSKY, fHeight);
				fRevH = fHeight - m_vPosition._42;

				m_vPosition._42 += fRevH;
				m_vWorld._42 += fRevH;
			}
		}

		bFALL = Fall(&vMOVE);

		if (!IsJump() && !IsFall() && !bFALL && !IsPush())
		{
			D3DXVECTOR2 vTDIR;

			m_vWorld._42 += fHeight - m_vPosition._42;
			m_vPosition._42 = fHeight;

			if (!pFLOOR && CheckFall(pMAP, &vTDIR))
			{
				m_fMoveSpeed = TMINFALL_SPEED;
				Fall(&vTDIR);
			}
		}
		else if (dwTick && m_vPosition._42 <= pPREV->y + fRevH && (m_vPosition._42 <= fHeight || bLAND))
		{
			m_vWorld._42 += fHeight - m_vPosition._42;
			m_vPosition._42 = fHeight;

			if (IsJump() || IsFall())
			{
				if (IsJump())
					m_dwDropDamage = CalcJumpDamage();
				else
					m_dwDropDamage = CalcFallDamage();

				m_bAction = GetTAction();
				m_bACTLevel = CTChart::GetTACTION(m_bAction)->m_bLevel;
			}
			else if (IsPush())
			{
				TACTION vActionID = FindActionID(
					TA_DOWN,
					GetWeaponID(m_bMode));

				m_dwDropDamage = CalcPushDamage();
				m_bAction = GetTAction();

				if (m_bACTLevel <= CTChart::GetTACTION(TA_DOWN)->m_bLevel)
					if (vActionID.m_dwActID && vActionID.m_dwAniID)
					{
						SetAction(vActionID.m_dwActID, vActionID.m_dwAniID);

						m_bACTLevel = CTChart::GetTACTION(TA_DOWN)->m_bLevel;
						m_bDown = TRUE;
					}
					else
						m_bACTLevel = CTChart::GetTACTION(m_bAction)->m_bLevel;
			}
			else
				m_bAction = GetTAction();
		}
		else if (dwTick && m_vPosition._42 <= fHeight)
		{
			FLOAT fMoveH = fHeight - m_vPosition._42;

			m_vPosition._42 = fHeight;
			m_vWorld._42 += fMoveH;

			if (IsJump())
				m_fJumpHeight += fMoveH;

			if (IsFall())
				m_fFallHeight += fMoveH;

			if (IsPush())
				m_fPushHeight += fMoveH;
		}

		LPTREGIONINFO pTREGION = pMAP ? pMAP->GetRegionINFO(
			m_vPosition._41,
			m_vPosition._43) : NULL;

		if (pTREGION)
			m_pTREGION = pTREGION;

		m_bDive = FALSE;

	}
	else



		CTClientMoveObj::CalcHeight(pPREV, pMAP, dwTick);
}
void CTClientSpolecnik::DoTRANS(CD3DDevice *pDevice,
	CTachyonRes *pRES,
	WORD wMonID)
{
	LPTMONTEMP pTMON = CTChart::FindTMONTEMP(wMonID);

	if (pTMON)
	{
		MAPTINVEN::iterator finder = m_mapTINVEN.find(INVEN_TRANS);
		CTClientInven *pTINVEN = NULL;

		if (finder == m_mapTINVEN.end())
		{
			pTINVEN = new CTClientInven();

			pTINVEN->m_bInvenID = INVEN_TRANS;
			pTINVEN->m_wItemID = 0;

			m_mapTINVEN.insert(MAPTINVEN::value_type(INVEN_TRANS, pTINVEN));
		}
		else
			pTINVEN = (*finder).second;

		pTINVEN->ClearInven();

		for (BYTE i = 0; i < ES_COUNT; i++)

		{
			if (pTMON->m_pTITEM[i])
			{
				CTClientItem *pTITEM = new CTClientItem();

				pTITEM->SetItemSlot(i);
				pTITEM->SetTITEM(pTMON->m_pTITEM[i]);
				pTITEM->SetCount(1);

				pTINVEN->m_mapTITEM.insert(make_pair(pTITEM->GetItemSlot(), pTITEM));
			}
		}

		InitOBJ(pRES->GetOBJ(pTMON->m_dwOBJ));
		m_pTRANS = pTMON;
		ResetEQUIP(pDevice, pRES);

		m_fBreathHeight = GetAttrFLOAT(ID_BREATH_HEIGHT);
		m_bCanSelected = FALSE;

		if (wMonID != PETBALL_MONID)
		{
			FLOAT fMax = pTMON->m_fPetSize;
			BYTE bLevel = m_bLevel;

			if (bLevel > MAX_PET_LEVEL)
				bLevel = MAX_PET_LEVEL;

			DrawEffect();

			FLOAT fMain = fMax / (GetAttrFLOAT(ID_SIZE_Y) * fMax) / MAX_PET_LEVEL;
			fMain *= bLevel + 9;

			FLOAT fCustomSize = CTClientGame::GetInstance()->FindCustomPetSize(wMonID);
			if (fCustomSize != -1.0f)
				fMain *= fCustomSize;

			D3DXMatrixScaling(
				&m_vScale,
				fMain,
				fMain,
				fMain);

			InitSIZE(
				fMain,
				pTMON->m_fPetSize,// * GetAttrFLOAT(ID_SIZE_Y),
				fMain,
				0.0f, 0.0f);

			m_vScaleSFX = CTMath::Inverse(&m_vScale);
			m_vPosition = m_vScale * m_vWorld;

			ResetRootID(ID_PIVOT_WAIST);
			pTMON->m_bCanFly = FALSE;
			m_pTRANS->m_bCanFly = FALSE;
		}
		else
		{
			FLOAT fMain = 1.5f;
			BYTE bLevel = m_bLevel;

			if (bLevel > MAX_PET_LEVEL)
				bLevel = MAX_PET_LEVEL;

			fMain += bLevel * 0.1f;

			D3DXMatrixScaling(
				&m_vScale,
				fMain,
				fMain,
				fMain);

			InitSIZE(
				0.1f,
				m_pTEMP->m_fScaleY * GetAttrFLOAT(ID_SIZE_Y),
				0.1f,
				0.0f,
				0.0f);

			m_vPreScale = m_vScale;
			m_vScaleSFX = CTMath::Inverse(&m_vScale);
			m_vPosition = m_vScale * m_vWorld;

			ResetRootID(ID_PIVOT_WAIST);

			GenerateSFX(
				&m_mapOBJSFXINST,
				&m_vOBJSFX,
				&m_mapOBJSFX,
				0.0f,
				GetPetBallID());
		}
	}
	PlayRawSFX(&m_vTGARBAGESFX, 287879749, TRUE);

	CTClientObjBase::DoTRANS(
		pDevice,
		pRES,
		wMonID);

}

void CTClientSpolecnik::DoRETRANS(CD3DDevice *pDevice,
	CTachyonRes *pRES)
{
	CTClientMoveObj::DoRETRANS(
		pDevice,
		pRES);

	m_bCanTeleport = FALSE;

	LPTMONTEMP pTMON = m_pTEMP;
	if (pTMON)
	{
		MAPTINVEN::iterator finder = m_mapTINVEN.find(INVEN_TRANS);
		CTClientInven *pTINVEN = NULL;

		if (finder == m_mapTINVEN.end())
		{
			pTINVEN = new CTClientInven();

			pTINVEN->m_bInvenID = INVEN_TRANS;
			pTINVEN->m_wItemID = 0;

			m_mapTINVEN.insert(MAPTINVEN::value_type(INVEN_TRANS, pTINVEN));
		}
		else
			pTINVEN = (*finder).second;

		pTINVEN->ClearInven();

		for (BYTE i = 0; i < ES_COUNT; i++)
		{
			if (pTMON->m_pTITEM[i])
			{
				CTClientItem *pTITEM = new CTClientItem();

				pTITEM->SetItemSlot(i);
				pTITEM->SetTITEM(pTMON->m_pTITEM[i]);
				pTITEM->SetCount(1);

				pTINVEN->m_mapTITEM.insert(make_pair(pTITEM->GetItemSlot(), pTITEM));
			}
		}

		InitOBJ(pRES->GetOBJ(pTMON->m_dwOBJ));
		m_pTRANS = NULL;
		ResetEQUIP(pDevice, pRES);

		m_fBreathHeight = GetAttrFLOAT(ID_BREATH_HEIGHT);
		m_bCanSelected = FALSE;

		FLOAT fMax = pTMON->m_fPetSize;

		BYTE bLevel = m_bLevel;

		if (bLevel > MAX_PET_LEVEL)
			bLevel = MAX_PET_LEVEL;

		//DrawEffect();

		FLOAT fMain = fMax / (GetAttrFLOAT(ID_SIZE_Y) * fMax) / MAX_PET_LEVEL;
		fMain *= bLevel + 9;

		FLOAT fCustomSize = CTClientGame::GetInstance()->FindCustomPetSize(pTMON->wMonID);
		if (fCustomSize != -1.0f)
			fMain *= fCustomSize;

		D3DXMatrixScaling(
			&m_vScale,
			fMain,
			fMain,
			fMain);

		InitSIZE(
			fMain,
			pTMON->m_fPetSize,// * GetAttrFLOAT(ID_SIZE_Y),
			fMain,
			0.0f, 0.0f);

		m_vScaleSFX = CTMath::Inverse(&m_vScale);
		m_vPosition = m_vScale * m_vWorld;
		m_vPosition._42 = 0.0f;
		m_vWorld._42 = 0.0f;
		m_bCanTeleport = TRUE;

		if (pTMON->wMonID >= 31250)
		{
			WORD wIndex = pTMON->wMonID - 31250;
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
			/*	case 10:
				case 11:
				{
					ApplyCustomTexture(25, 1);
				}
				break;*/
			}
		}
	}
	DrawEffect();


	PlayRawSFX(&m_vTGARBAGESFX, 287879749, TRUE);
}

void CTClientSpolecnik::SetTAction(BYTE bAction)
{
	CTClientObjBase::SetTAction(bAction);
}

void CTClientSpolecnik::SetEffect(BYTE bEffect)
{
	m_bEffect = bEffect;






}

LPTITEMGRADEVISUAL CTClientSpolecnik::GetPETVISUAL(BYTE m_bEffect)
{
	WORD wGradeSkin = 0;
	switch (m_bEffect)
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
	if (itr != CTChart::m_mapTITEMGRADEVISUAL.end())
		return &(itr->second);


	return NULL;
}

void CTClientSpolecnik::DrawEffect()
{
	if (m_bEffect > 0)
	{
		LPTITEMGRADEVISUAL pGradeVISUAL = GetPETVISUAL(m_bEffect);

		if (pGradeVISUAL &&  pGradeVISUAL->m_pSkinTex)
		{
			LPTEXTURESET pSkinTEX = pGradeVISUAL->m_pSkinTex;
			BYTE bPSTYPE = pGradeVISUAL->m_bPSTYPE;

			MAPOBJPART::iterator itr, end;
			itr = m_OBJ.m_mapDRAW.begin();
			end = m_OBJ.m_mapDRAW.end();

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
}
// ˇ GOOD CODE STARTS THERE ˇ //

BYTE CTClientSpolecnik::GetPetBallID()
{
	CTClientChar* pHOST = CTClientGame::GetInstance()->FindPC(GetHostID());
	if (!pHOST)
		return FALSE;

	switch (pHOST->m_bContryID)
	{
	case TCONTRY_C:
		return TBALLID_CRAXION;
	case TCONTRY_D:
		return TBALLID_DEFUGEL;
	default:
		return TBALLID_BROA;
	}

	return FALSE;
}