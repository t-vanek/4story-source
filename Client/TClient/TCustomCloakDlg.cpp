#include "stdafx.h"
#include "TCustomCloakDlg.h"
#include "TClientGame.h"
#include "TClientWnd.h"

#define TCUSTOMCLOAK_CAM_DIST		3.0f

CTCustomCloakDlg::CTCustomCloakDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc )
: CTClientUIBase( pParent, pDesc )
{
	m_pList = static_cast<TList*>( FindKid(26087) );
	m_p3D = FindKid(1284);


	D3DLIGHT9 light;
	m_pCHAR = new CTClientChar();
	D3DXMATRIX matINIT;
	D3DXMatrixIdentity(&matINIT);
	m_pCHAR->SetPosition(matINIT);
	m_pCHAR->m_vWorld = matINIT;

	ZeroMemory( &light, sizeof(D3DLIGHT9));

	light.Diffuse.r = 1.0f;
	light.Diffuse.g = 1.0f;
	light.Diffuse.b = 1.0f;
	light.Diffuse.a = 1.0f;

	light.Ambient.r = 0.0f;
	light.Ambient.g = 0.0f;
	light.Ambient.b = 0.0f;
	light.Ambient.a = 1.0f;

	light.Direction = D3DXVECTOR3( -0.1f, -0.1f, -1.0f);
	light.Type = D3DLIGHT_DIRECTIONAL;
	light.Range = T3DLIGHT_RANGE_MAX;

	m_vLIGHT[TLIGHT_CENTER].SetLight(&light);
	m_vLIGHT[TLIGHT_CENTER].EnableLight(FALSE);

	light.Direction = D3DXVECTOR3( -1.0f, -0.1f, 0.1f);
	light.Diffuse.r = 0.4f;
	light.Diffuse.g = 0.4f;
	light.Diffuse.b = 0.4f;
	light.Diffuse.a = 1.0f;

	m_vLIGHT[TLIGHT_SIDE].SetLight(&light);
	m_vLIGHT[TLIGHT_SIDE].EnableLight(FALSE);

	light.Direction = D3DXVECTOR3( 0.1f, 0.1f, 1.0f);
	light.Diffuse.r = 0.4f;
	light.Diffuse.g = 0.4f;
	light.Diffuse.b = 0.4f;
	light.Diffuse.a = 1.0f;

	m_vLIGHT[TLIGHT_BACK].SetLight(&light);
	m_vLIGHT[TLIGHT_BACK].EnableLight(FALSE);

	CTClientWnd* pMainWnd = CTClientWnd::GetInstance();
	m_pDevice = &pMainWnd->m_Device;
	
}

CTCustomCloakDlg::~CTCustomCloakDlg()
{	
	
}

// ====================================================================================================
void CTCustomCloakDlg::ShowComponent( BOOL bVisible)
{
	
	WORD wCharCloakID = 0;
	MAPTINVEN::iterator itTINVEN = m_pCHAR->m_mapTINVEN.find(INVEN_EQUIP);
		if( itTINVEN != m_pCHAR->m_mapTINVEN.end() )
		{
			CTClientInven *pTEQUIP = (*itTINVEN).second;
			if(pTEQUIP)
			{
				CTClientItem* pTITEM = pTEQUIP->FindTItem( ES_BACK );
				if( pTITEM )
				{
					wCharCloakID = pTITEM->GetCustomTex();
				}
			}
		}

	m_pList->m_nCurSel = 0;
	for(int i =0; i < m_pList->GetItemCount();i++)
	{
		WORD wID = (WORD)m_pList->GetItemData(i,0);
		if(wID == wCharCloakID)
		{
			m_pList->m_nCurSel = i;
		}
	}
	CTClientUIBase::ShowComponent(bVisible);
}
// ----------------------------------------------------------------------------------------------------
void CTCustomCloakDlg::AddCloak(WORD wID,__int64 dDateCreate)
{
	CTime t(dDateCreate);
	
	CString strFMT;
	strFMT.Format( "%d/%d/%d %d:%d",
		t.GetYear(),
		t.GetMonth(),
		t.GetDay(),
		t.GetHour(),
		t.GetMinute() );
	if(wID == 0)
		strFMT = "NONE";
		
	int nLine = m_pList->AddString(strFMT,0);
	m_pList->SetItemData(nLine, 0, wID);
}
void CTCustomCloakDlg::ClearCloaks()
{
	m_pList->RemoveAll();
}
void CTCustomCloakDlg::OnLButtonDown( UINT nFlags, CPoint pt )
{
	

	
	CTClientUIBase::OnLButtonDown( nFlags, pt );
	if(m_pList->HitTest(pt))
	{	
		ResetData(m_pCHAR,CTClientGame::GetInstance()->GetResource());
		/*MAPTINVEN::iterator itTINVEN = m_pCHAR->m_mapTINVEN.find(INVEN_EQUIP);
		if( itTINVEN != m_pCHAR->m_mapTINVEN.end() )
		{
			CTClientInven *pTEQUIP = (*itTINVEN).second;
			CTClientItem* pTITEM = pTEQUIP->FindTItem( ES_BACK );
			if( pTITEM )
			{
				if(m_pList->GetSel() != -1)		
				{					
					WORD wID = (WORD)m_pList->GetItemData( m_pList->GetSel(), 0 );					
					CTClientCustomCloak *pCustom =  new CTClientCustomCloak(wID);
					CTClientCustomCloak::SetMantleTexture(
						m_pCHAR,
						pCustom );
				}
			}
		}*/
	}
	
}

void CTCustomCloakDlg::EnableTLIGHT( CD3DCamera *pCamera,
								  BYTE bENABLE)
{
	for(auto i = 0; i<TLIGHT_COUNT; i++)
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

		for( auto i = 0; i<TLIGHT_COUNT; i++)
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

		for( auto i = 0; i<TLIGHT_COUNT; i++)
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

		for( auto i = 0; i<TLIGHT_COUNT; i++)
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


void CTCustomCloakDlg::ResetCHAR()
{
	m_pCHAR->Release();
}

void CTCustomCloakDlg::ResetData( CTClientChar *pCHAR,
							   CTachyonRes *pRES)
{
	BYTE bRaceID = pCHAR->GetRaceID();

	LPOBJECT pNEXT = pRES->GetOBJ( CTChart::m_vTRACE[ bRaceID ][pCHAR->m_bSex]);
	LPOBJECT pPREV = m_pCHAR->m_OBJ.m_pOBJ;

	m_pCHAR->m_bRaceID_ = bRaceID;
	m_pCHAR->m_bSex = pCHAR->m_bSex;

	m_pCHAR->m_bPants = pCHAR->m_bPants;
	m_pCHAR->m_bHair = pCHAR->m_bHair;
	m_pCHAR->m_bFace = pCHAR->m_bFace;
	m_pCHAR->m_bBody = pCHAR->m_bBody;
	m_pCHAR->m_bHand = pCHAR->m_bHand;
	m_pCHAR->m_bFoot = pCHAR->m_bFoot;
	m_pCHAR->m_bEquipMode = pCHAR->m_bEquipMode;
	m_pCHAR->m_bHelmetHide = pCHAR->m_bHelmetHide;
	m_pCHAR->m_bShowCloakCustume = TRUE;

	if( pPREV != pNEXT )
	{
		m_pCHAR->InitOBJ(pNEXT);
		m_pCHAR->m_fSizeY = m_pCHAR->GetAttrFLOAT(ID_SIZE_Y);
	}

	// jkchoi
	if( m_pCHAR->m_pGuildMark )
		delete m_pCHAR->m_pGuildMark;
	m_pCHAR->m_pGuildMark = NULL;

	if( pCHAR->m_pGuildMark )
	{
		m_pCHAR->m_pGuildMark = new CTClientGuildMark( *pCHAR->m_pGuildMark );
	}

	MAPTINVEN::iterator finder = pCHAR->m_mapTINVEN.find(INVEN_EQUIP);
	if( finder != pCHAR->m_mapTINVEN.end() )
	{
		m_pCHAR->m_mapTINVEN.insert( MAPTINVEN::value_type( INVEN_EQUIP, (*finder).second));
		m_pCHAR->ClearEquip();
		m_pCHAR->ResetEQUIP( m_pDevice, pRES);
		m_pCHAR->m_mapTINVEN.clear();		

	}

	if(m_pList->GetSel() != -1)		
	{					
		WORD wID = (WORD)m_pList->GetItemData( m_pList->GetSel(), 0 );					
		CTClientCustomCloak pCustom =  CTClientCustomCloak(wID);
		CTClientCustomCloak::SetMantleTexture(
			m_pCHAR,
			&pCustom );
	}
	

	TACTION vActionID = pCHAR->FindActionID(
		TA_STAND,
		WT_NORMAL);

	m_pCHAR->ClearAnimationID();
	m_pCHAR->SetAction(
		vActionID.m_dwActID,
		vActionID.m_dwAniID);

	
}

HRESULT CTCustomCloakDlg::Render(DWORD dwTickCount)
{
	TComponent *pCOMP = FindKid(ID_CTRLINST_3D);
	pCOMP->ShowComponent(FALSE);

	HRESULT hr = CTClientUIBase::Render(dwTickCount);
	if( IsVisible() )
	{
		CTClientChar* pMainChar = CTClientGame::GetInstance()->GetMainChar();
		m_pCHAR->m_bHelmetHide = pMainChar->m_bHelmetHide;
		m_pCHAR->m_bShowCloakCustume = TRUE;

		CTClientChar *pOBJ = m_pCHAR;
		
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

		FLOAT fHeight = m_pCHAR->m_fSizeY * FLOAT(vNEW.Height) / FLOAT(rect.Height()) * 1.10f;
		FLOAT fWidth = m_pCHAR->m_fSizeY * FLOAT(vNEW.Width) / FLOAT(rect.Height());

		m_vCamera.InitOrthoCamera(
			m_pDevice->m_pDevice,
			-TCUSTOMCLOAK_CAM_DIST,
			TCUSTOMCLOAK_CAM_DIST,
			fWidth,
			fHeight);

		fWidth = (m_pCHAR->m_fSizeY * FLOAT(rect.Width()) / FLOAT(rect.Height()) - fWidth) / 2.0f;
		fHeight /= 2.0f;

		m_vCamera.SetPosition(
			D3DXVECTOR3( fWidth, fHeight, +1.0f),
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
