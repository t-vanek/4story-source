#include "StdAfx.h"
#include "TCharInfoNewDlg.h"
#include "TClientGame.h"
#include "Resource.h"
#include "TCharInfoNewInner.h"

#define TCHARINFO_CAM_DIST		3.0f

CTCharNewDlg::CTCharNewDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc, TCMLParser *pParser, CD3DDevice *pDevice, WORD wFrameID, CTClientChar* pHost)
:CTInvenDlg( pParent, pDesc, wFrameID), m_pHost(pHost)
{
	static DWORD dwValueID[] = {
		27606,
		27607,
		27608,
		27609,
		27610,
		27611,
		27612,
		27613,
		27614,
		27615,
		27616,
		27617};

	static DWORD dwStatID[] = {
		26380,
		26381,
		26382,
		26383,
		26384,
		26385
	};

	static DWORD dwOtherComps[] = {
		28007,
		26354,
	    26370,
		26371,
		26372,
		26373,
		26374
	};

	static DWORD dwToDelete[] = {
		27356,
		27357,
		27358,
		27359,
		27360,
		27361
	};

	for(BYTE i=0;i<6;++i)
	{
		TComponent* pTrash = FindKid(dwToDelete[i]);
		pTrash->ShowComponent(FALSE);
		RemoveKid(pTrash);
		
		delete pTrash;
	}

	D3DLIGHT9 light;

	for(BYTE i=0; i<TCHARSTAT_COUNT; i++)
	{
		m_vTCHARSTAT[i] = FindKid(dwStatID[i]);
		m_vTCHARSTAT[i]->m_bWordBreak = TRUE;
	}

	for(auto i=0; i<TCHARBASIC_COUNT; i++)
	{
		m_vTCHARBASIC[i] = FindKid(dwValueID[i]);
		m_vTCHARBASIC[i]->m_bWordBreak = TRUE;

		if(TCHARBASIC_PSP == i)
		{
			CPoint pTemp;
			m_vTCHARBASIC[i]->GetComponentPos(&pTemp);
			pTemp.x += 3;

			m_vTCHARBASIC[i]->MoveComponent(pTemp);
		}
	}

	for(auto i=0; i<TCHARADV_COUNT; i++)
	{
		m_vTCHARADV[i] = FindKid(dwOtherComps[i]);
		m_vTCHARADV[i]->m_bWordBreak = TRUE;
	}

	m_pCOUNTRY = (TImageList*) FindKid(26353);
	m_pTHELMETHIDE = (TButton*)FindKid(ID_CTRLINST_BTN_HELMETHIDE);

	m_pDevice = pDevice;

	D3DXMATRIX matINIT;
	D3DXMatrixIdentity(&matINIT);
	m_vCHAR.SetPosition(matINIT);
	m_vCHAR.m_vWorld = matINIT;

	ZeroMemory( &light, sizeof(D3DLIGHT9));

	light.Diffuse.r = 1.0f;
	light.Diffuse.g = 1.0f;
	light.Diffuse.b = 1.0f;
	light.Diffuse.a = 1.0f;

	light.Ambient.r = 0.0f;
	light.Ambient.g = 0.0f;
	light.Ambient.b = 0.0f;
	light.Ambient.a = 1.0f;

	light.Direction = D3DXVECTOR3( -0.1f, -0.1f, 1.0f);
	light.Type = D3DLIGHT_DIRECTIONAL;
	light.Range = T3DLIGHT_RANGE_MAX;

	m_vLIGHT[TLIGHT_CENTER].SetLight(&light);
	m_vLIGHT[TLIGHT_CENTER].EnableLight(FALSE);

	light.Direction = D3DXVECTOR3( -1.0f, -0.1f, -0.1f);
	light.Diffuse.r = 0.4f;
	light.Diffuse.g = 0.4f;
	light.Diffuse.b = 0.4f;
	light.Diffuse.a = 1.0f;

	m_vLIGHT[TLIGHT_SIDE].SetLight(&light);
	m_vLIGHT[TLIGHT_SIDE].EnableLight(FALSE);

	light.Direction = D3DXVECTOR3( 0.1f, 0.1f, -1.0f);
	light.Diffuse.r = 0.4f;
	light.Diffuse.g = 0.4f;
	light.Diffuse.b = 0.4f;
	light.Diffuse.a = 1.0f;

	m_vLIGHT[TLIGHT_BACK].SetLight(&light);
	m_vLIGHT[TLIGHT_BACK].EnableLight(FALSE);

	static DWORD dwInvenID[] = {
		26355,
		26356,
		26357,
		26358,
		26359};

	for(auto i=0; i<MAX_SUBINVEN; ++i)
		m_pTSUBINVEN[i] = (TImageList *) FindKid(dwInvenID[i]);

	m_pTDEFINVEN = FindKid(ID_CTRLINST_DEFINVEN);

	TComponent* pCOMP = FindKid( 26360 );
	pCOMP->m_bWordBreak = FALSE;
	m_pCloakHide = new TButton(this, m_pTHELMETHIDE->m_pDESC);
	TComponent* pText = new TComponent(this, pCOMP->m_pDESC);
	CPoint pPos[2] = {};

	m_pCloakHide->m_id = GetUniqueID();
	pText->m_id = GetUniqueID();

	pText->m_bWordBreak = FALSE;
	pText->m_strText = "Hide Cloak";

	m_pTHELMETHIDE->GetComponentPos(&pPos[0]);
	pCOMP->GetComponentPos(&pPos[1]);

	m_pCloakHide->MoveComponent(CPoint(pPos[0].x, pPos[0].y - 17));
	pText->MoveComponent(CPoint(pPos[1].x, pPos[1].y - 17));

	m_pCloakHide->m_menu[TNM_LCLICK] = GM_TOGGLE_CLOAKHIDE;

	AddKid(m_pCloakHide);
	AddKid(pText);
	
}

CTCharNewDlg::~CTCharNewDlg()
{
	m_vCHAR.Release();
}


void CTCharNewDlg::ResetCHAR()
{
	m_vCHAR.Release();
}

void CTCharNewDlg::ResetData( CTClientChar *pCHAR,
							   CTachyonRes *pRES,
							   BYTE bTabIndex)
{
	static DWORD dwTextID[TINFO_COUNT][TCHARPANE_COUNT] = {
		{ TSTR_TCHARINFO_PHYSIC, TSTR_TCHARINFO_MAGIC},
		{ TSTR_TCHARINFO_RANK, 0},
		{ 0, 0 },
		{ 0, 0 },
		{ 0, 0 }
	};

	BYTE bRaceID = pCHAR->GetRaceID();

	LPOBJECT pNEXT = pRES->GetOBJ( CTChart::m_vTRACE[ bRaceID ][pCHAR->m_bSex]);
	LPOBJECT pPREV = m_vCHAR.m_OBJ.m_pOBJ;

	m_vCHAR.m_bRaceID_ = bRaceID;
	m_vCHAR.m_bSex = pCHAR->m_bSex;

	m_vCHAR.m_bPants = pCHAR->m_bPants;
	m_vCHAR.m_bHair = pCHAR->m_bHair;
	m_vCHAR.m_bFace = pCHAR->m_bFace;
	m_vCHAR.m_bBody = pCHAR->m_bBody;
	m_vCHAR.m_bHand = pCHAR->m_bHand;
	m_vCHAR.m_bFoot = pCHAR->m_bFoot;
	m_vCHAR.m_bEquipMode = pCHAR->m_bEquipMode;
	m_vCHAR.m_bHelmetHide = pCHAR->m_bHelmetHide;
	m_vCHAR.m_bCloakHide = pCHAR->m_bCloakHide;

	if( pPREV != pNEXT )
	{
		m_vCHAR.InitOBJ(pNEXT);
		m_vCHAR.m_fSizeY = m_vCHAR.GetAttrFLOAT(ID_SIZE_Y);
	}

	if( m_vCHAR.m_pGuildMark )
		delete m_vCHAR.m_pGuildMark;
	m_vCHAR.m_pGuildMark = NULL;

	if( pCHAR->m_pGuildMark )
	{
		m_vCHAR.m_pGuildMark = new CTClientGuildMark( *pCHAR->m_pGuildMark );
	}

	MAPTINVEN::iterator finder = pCHAR->m_mapTINVEN.find(INVEN_EQUIP);
	if( finder != pCHAR->m_mapTINVEN.end() )
	{
		m_vCHAR.m_mapTINVEN.insert( MAPTINVEN::value_type( INVEN_EQUIP, (*finder).second));
		m_vCHAR.ClearEquip();
		m_vCHAR.ResetEQUIP( m_pDevice, pRES);
		m_vCHAR.m_mapTINVEN.clear();
		m_pTINVEN = (*finder).second;
	}

	TACTION vActionID = pCHAR->FindActionID(
		TA_STAND,
		WT_NORMAL);

	m_vCHAR.ClearAnimationID();
	m_vCHAR.SetAction(
		vActionID.m_dwActID,
		vActionID.m_dwAniID);
}

void CTCharNewDlg::ResetStatColor( CTClientChar *pCHAR)
{
	for( INT i=0 ; i < TCHARSTAT_COUNT ; ++ i)
        pCHAR->m_StatInfo.m_bColorSTAT[ i ] = 0;

	for( INT i=0 ; i < TCHARBASIC_COUNT ; ++ i)
		pCHAR->m_StatInfo.m_bColorBASIC[ i ] = 0;

	MAPTINVEN::iterator itINVEN = pCHAR->m_mapTINVEN.find( INVEN_EQUIP );
	if( itINVEN == pCHAR->m_mapTINVEN.end() )
		return ;

	CTClientInven* pTINVEN = itINVEN->second;
	if( pTINVEN )
	{
		MAPTITEM::iterator it, end;
		it = pTINVEN->m_mapTITEM.begin();
		end = pTINVEN->m_mapTITEM.end();

		for(; it != end ; ++it )
		{
			CTClientItem* pTITEM = it->second;

			if( !pTITEM )
				continue;

			if( pTITEM->GetGrade() != 0 )
			{
				WORD wAttrID = pTITEM->GetAttrID();
				wAttrID += CTChart::m_vTITEMGRADE[ pTITEM->GetGrade() ].m_bGrade;

				LPTITEMATTR pTITEMATTR = CTChart::FindTITEMATTR( wAttrID );
				if( pTITEMATTR )
				{
					if( pTITEMATTR->m_wMinAP || pTITEMATTR->m_wMaxAP  ) // 최소 물리 공격력, 최대 물리 공격력
						pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_PAP ] = TRUE;

					if( pTITEMATTR->m_wDP ) // 물리 방어력
						pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_PDP ] = TRUE;

					if( pTITEMATTR->m_wMinMAP || pTITEMATTR->m_wMaxMAP ) // 최소 마법 공격력, 최대 마법 공격력
						pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_MAP ] = TRUE;

					if( pTITEMATTR->m_wMDP ) // 마법 방어력
						pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_MDP ] = TRUE;
				}
			}

			MAPTMAGIC& vMAGIC = *( pTITEM->GetTMAGIC());
			MAPTMAGIC::iterator itMAGIC, endMAGIC;
			itMAGIC = vMAGIC.begin();
			endMAGIC = vMAGIC.end();

			for(; itMAGIC != endMAGIC ; ++itMAGIC )
			{
				BYTE bMagicID = itMAGIC->first;

				switch( bMagicID )
				{
				case 1: pCHAR->m_StatInfo.m_bColorSTAT[ TCHARSTAT_STR ] = TRUE; break; // 힘
				case 2: pCHAR->m_StatInfo.m_bColorSTAT[ TCHARSTAT_DEX ] = TRUE; break; // 민첩
				case 3: pCHAR->m_StatInfo.m_bColorSTAT[ TCHARSTAT_CON ] = TRUE; break; // 체력
				case 4: pCHAR->m_StatInfo.m_bColorSTAT[ TCHARSTAT_INT ] = TRUE; break; // 지능
				case 5: pCHAR->m_StatInfo.m_bColorSTAT[ TCHARSTAT_WIS ] = TRUE; break; // 지혜
				case 6: pCHAR->m_StatInfo.m_bColorSTAT[ TCHARSTAT_MEN ] = TRUE; break; // 정신
				case 7: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_PAP ] = TRUE; break; // 물리 공격력
				case 8: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_PDP ] = TRUE; break; // 물리 방어력
				case 9: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_PSP ] = TRUE; break; // 원거리 공격력
				case 11: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_PAL ] = TRUE; break; // 물리 공격등급
				case 12: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_PDL ] = TRUE; break; // 물리 방어등급
				case 13: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_PCR ] = TRUE; break; // 물리 치명 확률
				case 17: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_MAP ] = TRUE; break; // 마법 공격력
				case 16: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_MDP ] = TRUE; break; // 마법 방어력
				case 20: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_STA ] = TRUE; break; //마법 시전 유지확률
				case 86: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_MAL ] = TRUE; break; // 마법 공격등급
				case 87: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_MDL ] = TRUE; break; // 마법 방어등급
				case 21: pCHAR->m_StatInfo.m_bColorBASIC[ TCHARBASIC_MCR ] = TRUE; break; // 마법 치명 확률
				}
			}
		}
	}

	for( INT i=0 ; i < TCHARSTAT_COUNT ; ++i )
		m_vTCHARSTAT[ i ]->m_pFont->m_dwColor = pCHAR->m_StatInfo.m_bColorSTAT[ i ] ? TSTATCOLOR_BLUE : TSTATCOLOR_WHITE;

	for( INT i=0 ; i < TCHARBASIC_COUNT ; ++i )
		m_vTCHARBASIC[ i ]->m_pFont->m_dwColor = pCHAR->m_StatInfo.m_bColorBASIC[ i ] ? TSTATCOLOR_BLUE : TSTATCOLOR_WHITE;
}

void CTCharNewDlg::EnableTLIGHT( CD3DCamera *pCamera,
								  BYTE bENABLE)
{
	for( int i=0; i<TLIGHT_COUNT; i++)
		m_vLIGHT[i].EnableLight(bENABLE);

	if(!m_pDevice->m_bEnableSHADER)
		return;

	if(bENABLE)
	{
		static int vLightCount[4] = { TLIGHT_COUNT, 0, 1, 0};
		FLOAT vCONST[16];

		m_pDevice->m_pDevice->SetVertexShaderConstantI(
			m_pDevice->m_vConstantVS[VC_LIGHTCOUNT],
			vLightCount, 1);

		m_pDevice->m_pDevice->SetVertexShaderConstantF(
			m_pDevice->m_vConstantVS[VC_CAMPOS],
			(FLOAT *) &D3DXVECTOR4(
			pCamera->m_vPosition.x,
			pCamera->m_vPosition.y,
			pCamera->m_vPosition.z,
			0.0f), 1);

		D3DXMatrixTranspose( (LPD3DXMATRIX) vCONST, &(pCamera->m_matView * pCamera->m_matProjection));
		m_pDevice->m_pDevice->SetVertexShaderConstantF(
			m_pDevice->m_vConstantVS[VC_PROJ],
			vCONST, 4);

		for(auto i=0; i<TLIGHT_COUNT; i++)
		{
			memcpy( &vCONST[i * 4], &D3DXVECTOR4(
				m_vLIGHT[i].m_Light.Ambient.r,
				m_vLIGHT[i].m_Light.Ambient.g,
				m_vLIGHT[i].m_Light.Ambient.b,
				m_vLIGHT[i].m_Light.Ambient.a),
				4 * sizeof(FLOAT));
		}

		m_pDevice->m_pDevice->SetVertexShaderConstantF(
			m_pDevice->m_vConstantVS[VC_LIGHTAMBIENT],
			vCONST, TLIGHT_COUNT);

		for(auto i=0; i<TLIGHT_COUNT; i++)
		{
			memcpy( &vCONST[i * 4], &D3DXVECTOR4(
				m_vLIGHT[i].m_Light.Diffuse.r,
				m_vLIGHT[i].m_Light.Diffuse.g,
				m_vLIGHT[i].m_Light.Diffuse.b,
				m_vLIGHT[i].m_Light.Diffuse.a),
				4 * sizeof(FLOAT));
		}

		m_pDevice->m_pDevice->SetVertexShaderConstantF(
			m_pDevice->m_vConstantVS[VC_LIGHTDIFFUSE],
			vCONST, TLIGHT_COUNT);

		for(auto i=0; i<TLIGHT_COUNT; i++)
		{
			memcpy( &vCONST[i * 4], &D3DXVECTOR4(
				m_vLIGHT[i].m_Light.Direction.x,
				m_vLIGHT[i].m_Light.Direction.y,
				m_vLIGHT[i].m_Light.Direction.z,
				1.0f),
				4 * sizeof(FLOAT));
		}

		m_pDevice->m_pDevice->SetVertexShaderConstantF(
			m_pDevice->m_vConstantVS[VC_LIGHTDIR],
			vCONST, TLIGHT_COUNT);
	}
	else
	{
		static int vLightCount[4] = { 0, 0, 1, 0};

		m_pDevice->m_pDevice->SetVertexShaderConstantI(
			m_pDevice->m_vConstantVS[VC_LIGHTCOUNT],
			vLightCount, 1);
	}
}

HRESULT CTCharNewDlg::Render( DWORD dwTickCount)
{
	if( IsVisible() )
	{
		CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();

		m_vTCHARADV[TCHARADV_NAME]->m_strText = pMainChar->GetName();
		m_vTCHARADV[TCHARADV_LEVEL]->m_strText.Format("Lv%d", pMainChar->m_bLevel);
		m_vTCHARADV[TCHARADV_GUILD]->m_strText = pMainChar->m_strGUILD;
		m_vTCHARADV[TCHARADV_SOULMATE]->m_strText = CTClientGame::GetInstance()->GetSoulName();
		m_pCOUNTRY->SetCurImage(pMainChar->m_bContryID);

		if( pMainChar->m_bAidCountryID != TCONTRY_N )
		{
			DWORD dwRemainSecondChangeAID = CTClientGame::GetInstance()->m_dwRemainSecondChangeAID;
			dwRemainSecondChangeAID /= 1000;
			if( dwRemainSecondChangeAID == 0 )
			{
				m_vTCHARADV[TCHARADV_AID_AVAILABLE]->m_strText = CTChart::LoadString( TSTR_NATIONALITY_AVAILABLE ); // (변경 가능)
			}
			else
			{
				CString strFMT;
				DWORD dwSec = 0;
				DWORD dwMin = 0;
				DWORD dwHour = 0;
				DWORD dwDay = 0;
				
				CTClientGame::GetSecondToRemainTime( dwRemainSecondChangeAID, &dwDay, &dwHour, &dwMin, &dwSec);
				
				if( dwDay != 0 ) strFMT = CTChart::Format(TSTR_NATIONALITY_WAIT_DAY_STAT, dwDay ); // (%d일 이후 변경 가능)
				else if( dwHour != 0 ) strFMT = CTChart::Format(TSTR_NATIONALITY_WAIT_HOUR_STAT, dwHour );// (%d시간 이후 변경 가능)
				else if( dwMin != 0 ) strFMT = CTChart::Format(TSTR_NATIONALITY_WAIT_MIN_STAT, dwMin ); // (%d분 이후 변경 가능)
				else if( dwSec != 0 ) strFMT = CTChart::Format( TSTR_NATIONALITY_WAIT_SEC_STAT, dwSec ); //(%d초 이후 변경 가능)

				m_vTCHARADV[TCHARADV_AID_AVAILABLE]->m_strText = strFMT;
			}
		}
		else
		{
			m_vTCHARADV[TCHARADV_AID_AVAILABLE]->m_strText.Empty();
		}
	}

	TComponent *pCOMP = FindKid(ID_CTRLINST_3D);
	pCOMP->ShowComponent(FALSE);

	HRESULT hr = CTInvenDlg::Render(dwTickCount);
	if( IsVisible() )
	{
		CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();
		m_vCHAR.m_bHelmetHide = pMainChar->m_bHelmetHide;
		BOOL bHelmetHide = (pMainChar->m_bHelmetHide == ALL_HIDDEN || pMainChar->m_bHelmetHide != HELMET_SHOWN) && pMainChar->m_bHelmetHide != ALL_SHOWN ? FALSE : TRUE;
		BOOL bCloakHide = (pMainChar->m_bHelmetHide == ALL_HIDDEN || pMainChar->m_bHelmetHide != CLOAK_SHOWN) && pMainChar->m_bHelmetHide != ALL_SHOWN ? FALSE : TRUE;

		m_pTHELMETHIDE->Select(!bHelmetHide);
		m_pCloakHide->Select(!bCloakHide);

		CTClientChar *pOBJ = &m_vCHAR;
		
		D3DVIEWPORT9 vOLD;
		D3DVIEWPORT9 vNEW;
		CRect rect;

		pCOMP->GetComponentRect(&rect);
		pCOMP->ComponentToScreen(&rect);

		m_pDevice->m_pDevice->GetViewport(&vOLD);
		vNEW.Height = rect.Height() + min( 0, rect.top);
		vNEW.Width = rect.Width() + min( 0, rect.left);
		vNEW.MinZ = 0.0f;
		vNEW.MaxZ = 1.0f;
		vNEW.X = max( 0, rect.left);
		vNEW.Y = max( 0, rect.top);
		m_pDevice->m_pDevice->SetViewport(&vNEW);

		FLOAT fHeight = m_vCHAR.m_fSizeY * FLOAT(vNEW.Height) / FLOAT(rect.Height()) * 1.10f;
		FLOAT fWidth = m_vCHAR.m_fSizeY * FLOAT(vNEW.Width) / FLOAT(rect.Height());

		m_vCamera.InitOrthoCamera(
			m_pDevice->m_pDevice,
			-TCHARINFO_CAM_DIST,
			TCHARINFO_CAM_DIST,
			fWidth,
			fHeight);

		fWidth = (m_vCHAR.m_fSizeY * FLOAT(rect.Width()) / FLOAT(rect.Height()) - fWidth) / 2.0f;
		fHeight /= 2.0f;

		m_vCamera.SetPosition(
			D3DXVECTOR3( fWidth, fHeight, -1.0f),
			D3DXVECTOR3( fWidth, fHeight,  0.0f),
			D3DXVECTOR3(   0.0f,	1.0f,  0.0f),
			FALSE);

		m_vCamera.Rotate( 0.0f, -D3DX_PI / 18.0f, 0.0f);
		m_vCamera.Move(
			fWidth - m_vCamera.m_vTarget.x,
			fHeight - m_vCamera.m_vTarget.y,
			-m_vCamera.m_vTarget.z, TRUE);
		m_vCamera.Activate(TRUE);

		m_pDevice->m_pDevice->Clear(
			0, NULL,
			D3DCLEAR_ZBUFFER,
			0, 1.0f, 0);
		EnableTLIGHT( &m_vCamera, TRUE);

		static_cast<CTachyonObject*>( pOBJ )->CalcTick(
			m_pDevice->m_pDevice,
			dwTickCount);

		CTachyonMesh::BeginGlobalDraw(m_pDevice->m_pDevice);
		pOBJ->Render(
			m_pDevice,
			&m_vCamera);

		pOBJ->RenderWeaponEffect(
			m_pDevice,
			&m_vCamera );

		CTachyonMesh::EndGlobalDraw(m_pDevice->m_pDevice);

		m_pDevice->m_pDevice->SetViewport(&vOLD);
		EnableTLIGHT( &m_vCamera, FALSE);
	}

	return hr;
}

BOOL CTCharNewDlg::GetTChatInfo(const CPoint& point, TCHATINFO& outInfo )
{
	return CTInvenDlg::GetTChatInfo(point, outInfo );
}

ITDetailInfoPtr CTCharNewDlg::GetTInfoKey( const CPoint& point )
{
	CTCharInfoNewDlg* pChar = static_cast<CTCharInfoNewDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_CHARINFO_INNER));

	static const DWORD EQUIPINFOIDS[ES_COUNT] = 
	{
		32001,					// ES_PRMWEAPON
		32002,					// ES_SNDWEAPON
		32003,					// ES_LONGWEAPON
		32004,					// ES_HEAD
		32005,					// ES_BACK
		32006,					// ES_BODY
		32007,					// ES_PANTS
		32008,					// ES_FOOT
		32009,					// ES_HAND
		32010,					// ES_NECK
		32011,					// ES_LFINGER
		32012,					// ES_RFINGER
		32013,					// ES_LEAR
		32014,					// ES_REAR
		32015,					// ES_FACE
		32016,					// ES_COSTUME_HAT
		32017,					// ES_COSTUME_DRESS
		32018,					// ES_COSTUME_ACCESSARY
		32019						// ES_COSTUME_RACE
	};

	static const DWORD dwInfoID[ES_COUNT] = {
		TSTR_TEXT_PRMWEAPON,		// ES_PRMWEAPON
		TSTR_TEXT_SNDWEAPON,		// ES_SNDWEAPON
		TSTR_TEXT_LONGWEAPON,	// ES_LONGWEAPON
		TSTR_TEXT_HEAD,			// ES_HEAD
		TSTR_TEXT_BACK,			// ES_BACK
		TSTR_TEXT_BODY,			// ES_BODY
		TSTR_TEXT_PANTS,			// ES_PANTS
		TSTR_TEXT_FOOT,			// ES_FOOT
		TSTR_TEXT_HAND,			// ES_HAND
		TSTR_TEXT_NECK,			// ES_NECK
		TSTR_TEXT_RING,			// ES_LFINGER
		TSTR_TEXT_RING,			// ES_RFINGER
		TSTR_TEXT_EARING,		// ES_LEAR
		TSTR_TEXT_EARING,		// ES_REAR
		TSTR_TEXT_FACE,			// ES_FACE
		TSTR_TEXT_COSTUME_HAT,		// ES_COSTUME_HAT
		TSTR_TEXT_COSTUME_DRESS,		// ES_COSTUME_DRESS
		TSTR_TEXT_COSTUME_ACCESSARY,		// ES_COSTUME_ACCESSARY
		TSTR_TEXT_COSTUME_RACE,		// ES_COSTUME_RACE
	};

	static const DWORD dwStatInfoID[TCHARSTAT_COUNT] = {
		90001, //TCHARSTAT_STR 힘
		90002, //TCHARSTAT_DEX 민첩
		90003, //TCHARSTAT_CON 체력
		90004, //TCHARSTAT_INT 지능
		90005, //TCHARSTAT_WIS 지혜
		90006, //TCHARSTAT_MEN 정신
	};

	static const DWORD dwBasicInfoID[TCHARBASIC_COUNT] = {
		90007, //TCHARBASIC_PAP 물리 공격력
		90008, //TCHARBASIC_PDP	 물리 방어력
		90009, //TCHARBASIC_PSP 물리 원거리 공격력
		90010, //TCHARBASIC_PAL	 물리 공격등급
		90011, //TCHARBASIC_PDL	 물리 방어등급
		90012, //TCHARBASIC_PCR	 물리 치명확률
		90013, //TCHARBASIC_MAP 마법 공격력
		90014, //TCHARBASIC_MDP 마법 방어력
		90015, //TCHARBASIC_STA 마법 시전 유지확률
		90016, //TCHARBASIC_MAL 마법 공격등급
		90017, //TCHARBASIC_MDL 마법 방어등급
		90018, //TCHARBASIC_MCR 마법 치명확률
	};

	ITDetailInfoPtr pInfo;

	pInfo = GetTInfoKey_Inven(point, INVEN_DEFAULT);
	if( pInfo )
		return pInfo;

	for( INT i=0; i<MAX_SUBINVEN; ++i )
	{
		pInfo = GetTInfoKey_Inven(point, i);
		if( pInfo )
			return pInfo;
	}

	pInfo = CTInvenDlg::GetTInfoKey(point);

	if( !pInfo )
	{
		for( BYTE i=0; i<ES_COUNT; i++)
		{
			TComponent *pTICON = FindKid(CTClientGame::m_vSlotID[i]);

			if( pTICON )
			{
				CRect rc;
				pTICON->GetComponentRect(&rc);
				pTICON->ComponentToScreen(&rc);

				if(rc.PtInRect(point))
				{
					pChar->GetComponentRect(&rc);

					CString strTITLE;
					strTITLE = CTChart::LoadString( (TSTRING) dwInfoID[i]);
				 	pInfo = CTDetailInfoManager::NewNorInst(
						strTITLE, 0, 0, rc);

					pInfo->SetDir(TRUE, TRUE, TRUE, TRUE);

					return pInfo;
				}
			}
		}

		for( INT i=0 ; i < TCHARSTAT_COUNT ; ++i )
		{
			if( m_vTCHARSTAT[ i ]->HitRect( point ) )
			{
				CRect rc;
				pChar->GetComponentRect(&rc);

				DWORD dwTitleColor = TNEXTLEV_TEXT_COLOR;
				
				pInfo = CTDetailInfoManager::NewStatInst(
					dwStatInfoID[ i ],
					&(m_pHost->m_StatInfo),
					dwTitleColor,
					rc);

				pInfo->SetDir(TRUE, TRUE, TRUE, TRUE);

				return pInfo;
			}
		}

		for( INT i=0 ; i < TCHARBASIC_COUNT ; ++i )
		{
			if( m_vTCHARBASIC[ i ]->HitRect( point ) )
			{
				CRect rc;
				pChar->GetComponentRect(&rc);

				DWORD dwTitleColor = TNEXTLEV_TEXT_COLOR;

				pInfo = CTDetailInfoManager::NewStatInst(
					dwBasicInfoID[ i ],
					&(m_pHost->m_StatInfo),
					dwTitleColor,
					rc);

				pInfo->SetDir(TRUE, TRUE, TRUE, TRUE);

				return pInfo;
			}

		}
	}

	return pInfo;
}

BOOL CTCharNewDlg::CanWithItemUI()
{
	return FALSE;
}

void CTCharNewDlg::OnLButtonDown( UINT nFlags, CPoint pt)
{
	CTInvenDlg::OnLButtonDown( nFlags, pt);
}

void CTCharNewDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
	CTClientGame* pGame = CTClientGame::GetInstance();

	if( !pGame )
	{
		CTInvenDlg::OnLButtonUp( nFlags, pt);
		return ;
	}

	for( INT i=0 ; i < MAX_SUBINVEN ; ++i )
	{
		if( m_pTSUBINVEN[ i ]->HitTest( pt ) )
		{
			static BYTE bFrameID[MAX_SUBINVEN] = {
				TFRAME_INVEN_1,
				TFRAME_INVEN_2,
				TFRAME_INVEN_3,
				TFRAME_INVEN_4,
				TFRAME_INVEN_5};

			pGame->ToggleUI(bFrameID[i]);
			CTInvenDlg::OnLButtonUp( nFlags, pt);
			return ;
		}
	}

	if( m_pTDEFINVEN->HitTest( pt ) )
		pGame->OnGM_TOGGLE_DEFINVEN();

	CTInvenDlg::OnLButtonUp( nFlags, pt);
}

void CTCharNewDlg::OnMouseMove(UINT nFlags, CPoint pt)
{
	CTInvenDlg::OnMouseMove( nFlags, pt);
}

TDROPINFO CTCharNewDlg::OnDrop( CPoint point)
{
	if(m_bDropTarget)
	{
		TDROPINFO vResult;

		for( BYTE i=0; i<MAX_SUBINVEN; ++i)
		{
			if( !m_pTSUBINVEN[i]->HitTest(point) )
				continue;

			vResult.m_bSlotID = CTMainUI::MakeSlotID(TMAINSLOTID_INVEN, i);
			vResult.m_bDrop = TRUE;

			return vResult;
		}
	}

	return CTInvenDlg::OnDrop( point );
}

BYTE CTCharNewDlg::OnBeginDrag( LPTDRAG pDRAG, CPoint point)
{
	for( BYTE i=0; i<MAX_SUBINVEN; ++i)
	{
		if( !m_pTSUBINVEN[i]->HitTest(point) )
			continue;

		if( pDRAG )
		{
			CPoint pt = point;
			CSize size;

			pDRAG->m_pIMAGE = new TImageList(
				NULL,
				*m_pTSUBINVEN[i]);

			pDRAG->m_pIMAGE->SetCurImage(m_pTSUBINVEN[i]->GetCurImage());
			pDRAG->m_bSlotID = CTMainUI::MakeSlotID(TMAINSLOTID_INVEN, i);

			pDRAG->m_pIMAGE->GetComponentSize(&size);
			pt.x -= size.cx / 2;
			pt.y -= size.cy / 2;

			pDRAG->m_pIMAGE->ShowComponent(FALSE);
			pDRAG->m_pIMAGE->MoveComponent(pt);
		}

		return TRUE;
	}

	return CTInvenDlg::OnBeginDrag( pDRAG, point );
}

ITDetailInfoPtr CTCharNewDlg::GetTInfoKey_Inven(const CPoint& point, INT nID)
{
	ITDetailInfoPtr pInfo;
	CTCharInfoNewDlg* pChar = static_cast<CTCharInfoNewDlg*>(CTClientGame::GetInstance()->GetFrame(TFRAME_CHARINFO_INNER));

	if( nID == INVEN_DEFAULT )
	{
		if( !m_pTDEFINVEN->HitTest(point) )
			return pInfo;
	}
	else
	{
		if( !m_pTSUBINVEN[nID]->HitTest(point) )
			return pInfo;
	}

	MAPTINVEN::iterator find = m_pHost->m_mapTINVEN.find( nID );
	if( find != m_pHost->m_mapTINVEN.end() )
	{
		CTClientInven* pInven = find->second;
		LPTITEM pTITEM = pInven->m_pTITEM;

		CRect rc;
		pChar->GetComponentRect(&rc);

		pInfo = CTDetailInfoManager::NewItemInst(pTITEM, rc);
		pInfo->SetDir(TRUE, TRUE, TRUE, TRUE);
	}

	return pInfo;
}

void CTCharNewDlg::ResetSubInven( BYTE bInvenID, WORD wImageID, BYTE bEnable)
{
	BOOL m_bInvenSlot = TRUE;

	m_pTSUBINVEN[bInvenID]->EnableComponent(bEnable);
	m_pTSUBINVEN[bInvenID]->SetCurImage(wImageID);

	ResetVisible();
}

void CTCharNewDlg::ResetVisible()
{
	// 재정리 필요
	BOOL m_bInvenSlot = TRUE;
	BYTE nLastInven = 0;
}

void CTCharNewDlg::ShowComponent( BOOL bVisible )
{
	CTClientUIBase::ShowComponent( bVisible );
}

void CTCharNewDlg::ResetPosition()
{
	CTInvenDlg::ResetPosition();
}

void CTCharNewDlg::SetPvPPoint( DWORD dwPvPTotalPoint, DWORD dwPvPUseablePoint )
{
//	m_vTCHARADV[ TCHARADV_PVPTOTALPOINT]->m_strText.Format( _T("%d"), dwPvPTotalPoint );
//	m_vTCHARADV[ TCHARADV_PVPUSEABLEPOINT]->m_strText.Format( _T("%d"), dwPvPUseablePoint );
}