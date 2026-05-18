#include "Stdafx.h"
#include "TClientCustomCloak.h"
#include "TClientWnd.h"
#include "afxinet.h"

DWORD CTClientCustomCloak::m_dwLEFTTICK = TMAXGUILDMARK_MAKETICK;


BOOL CTClientCustomCloak::OnDeviceReset(BOOL bRelease)
{
	if (bRelease) // Release
	{
		MAPTCUSTOMCLOAKTEX::iterator it, end;
		it = m_mapTCUSTOMCLOAKTEX.begin();
		end = m_mapTCUSTOMCLOAKTEX.end();

		for (; it != end; ++it)
		{
			CT3DTexture *pTEX = it->second->GetTexturePtr(0);

			delete pTEX;
			it->second->ClearTexture();
		}
	}
	else // Restore
	{
		CTClientWnd* pMainWnd = CTClientWnd::GetInstance();
		LPDIRECT3DDEVICE9 pDevice = pMainWnd->m_Device.m_pDevice;
		pDevice->BeginScene();

		MAPTCUSTOMCLOAKTEX::iterator it, end;
		it = m_mapTCUSTOMCLOAKTEX.begin();
		end = m_mapTCUSTOMCLOAKTEX.end();

		for (; it != end; ++it)
		{
			LPTEXTURESET pTextureSet = GetMantleTexture(it->first);
			it->second->PushTexturePtr(pTextureSet->GetTexturePtr(0));
			delete pTextureSet;
		}

		pDevice->EndScene();
	}

	return TRUE;
}


// ====================================================================

// ====================================================================

// ====================================================================
CTClientCustomCloak::CTClientCustomCloak(WORD wID)
	: m_wID(wID)
{
	//LogFileWrite("cloack id %d",wID);
}
// --------------------------------------------------------------------
CTClientCustomCloak::CTClientCustomCloak(const CTClientCustomCloak& r)
{
	*this = r;
}
bool CTClientCustomCloak::operator < (const CTClientCustomCloak& r) const
{
	return m_wID < r.m_wID;
}
// --------------------------------------------------------------------
CTClientCustomCloak& CTClientCustomCloak::operator = (const CTClientCustomCloak& r)
{
	m_wID = r.m_wID;

	return *this;
}
// ====================================================================
void CTClientCustomCloak::SetMantleTexture(CTClientChar* pPlayer, CTClientCustomCloak* pMark)
{
	MAPCLKINST::iterator itCLK = pPlayer->m_OBJ.m_mapCLK.find(ID_CLK_BACK);
	if (itCLK == pPlayer->m_OBJ.m_mapCLK.end())
		return;

	if (itCLK->second->m_dwCurCL != ID_MANTLE)
		return;

	if (itCLK->second->m_pMESH &&
		itCLK->second->m_pMESH->m_pMESH)
	{
		CTachyonMesh* pMESH = itCLK->second->m_pMESH->m_pMESH;

		if( pMESH )
		{
			MAPOBJPART::iterator itDRAW, endDRAW;
			itDRAW = pPlayer->m_OBJ.m_mapDRAW.begin();
			endDRAW = pPlayer->m_OBJ.m_mapDRAW.end();

			for(; itDRAW != endDRAW ; ++itDRAW)
			{
				VECTOROBJPART *pDRAW = itDRAW->second;

				if( !pDRAW )
					continue;

				VECTOROBJPART::iterator itPART, endPART;
				itPART = pDRAW->begin();
				endPART = pDRAW->end();

				for(; itPART != endPART ; ++itPART)
				{
					LPOBJPART pPART = (*itPART);

					if( pPART &&
						pPART->m_pMESH == pMESH)
					{
						LPTEXTURESET pTextureSet = GetMantleTexture( pMark->m_wID );
						if( pTextureSet == NULL || pPART->m_pTEX->m_pTEX[1] == pTextureSet )
							continue;

						LPOBJTEX pNewTex = new OBJTEX;
						*pNewTex = *(pPART->m_pTEX);

						auto DefaultCloak = CTClientGame::GetInstance()->GetResource()->m_mapTEX.find(285966238);
						if (DefaultCloak != CTClientGame::GetInstance()->GetResource()->m_mapTEX.end())
							pNewTex->m_pTEX[0] = (LPTEXTURESET) DefaultCloak->second;

						pNewTex->m_bType[0] = TT_TEX;


						pNewTex->m_pTEX[1] = pTextureSet;//GetMantleTexture( pMark->m_wID );
						pNewTex->m_bType[1] = TT_TEX;
						pNewTex->m_dwOP = D3DTOP_MODULATE2X;
						pNewTex->m_dwCOLOR = D3DCOLOR_XRGB(0,0,0);

						if( pPART->m_bSelfDeleteTEX )
							delete pPART->m_pTEX;

						pPART->m_pTEX = pNewTex;
						pPART->m_bSelfDeleteTEX = TRUE;
					}
				}
			}
		}
	}
}
MAPTCUSTOMCLOAKTEX CTClientCustomCloak::m_mapTCUSTOMCLOAKTEX;

LPTEXTURESET CTClientCustomCloak::GetMantleTexture(
	WORD wID )
{
	if(wID == 0)
		return NULL;
	CTClientWnd* pMainWnd = CTClientWnd::GetInstance();
	LPDIRECT3DDEVICE9 pDevice = pMainWnd->m_Device.m_pDevice;

	MAPTCUSTOMCLOAKTEX::iterator it = m_mapTCUSTOMCLOAKTEX.find( wID );
	if( it != m_mapTCUSTOMCLOAKTEX.end() )
	{
		//LogFileWrite("%s 1",CTClientGame::GetInstance()->GetMainChar()->m_strNAME);
		return it->second;
	}
	else
	{
		if(pMainWnd->AddTCUSTOMCLOAKTEX(wID))
		{
			MAPTCUSTOMCLOAKTEX::iterator its = m_mapTCUSTOMCLOAKTEX.find( wID );
			if( its != m_mapTCUSTOMCLOAKTEX.end() )
			{
				return its->second;
			}
		}
		else
		{
			try
			{
				CString strURL;
				strURL.Format("http://90.180.197.162:81/cloaks/%d.png", wID);
				CString strPath;
				strPath.Format(".\\Data\\Cache\\%d", wID);
				URLDownloadToFile(nullptr, strURL, strPath, 0, nullptr);

				if (pMainWnd->AddTCUSTOMCLOAKTEX(wID))
				{
					MAPTCUSTOMCLOAKTEX::iterator its = m_mapTCUSTOMCLOAKTEX.find(wID);
					if (its != m_mapTCUSTOMCLOAKTEX.end())
					{
						return its->second;
					}
				}
			}
			catch (...)
			{
				LPDIRECT3DTEXTURE9 pTex;



				D3DXCreateTexture(
					pDevice,
					111,
					493, 1,
					D3DUSAGE_RENDERTARGET,
					D3DFMT_A8R8G8B8,
					D3DPOOL_DEFAULT,
					&pTex);


				LPTEXTURESET pTextureSet = new TEXTURESET;
				CT3DTexture *pT3DTEX = new CT3DTexture();

				pT3DTEX->m_pTDATA = (LPBYTE)pTex;
				pT3DTEX->m_bEnabled = TRUE;
				pTextureSet->PushTexturePtr(pT3DTEX);

				return pTextureSet;
			}
		}
	}

	return NULL;
}

void CTClientCustomCloak::ReleaseTMantleBakingTexture()
{
	MAPTCUSTOMCLOAKTEX::iterator it, end;
	it = m_mapTCUSTOMCLOAKTEX.begin();
	end = m_mapTCUSTOMCLOAKTEX.end();

	for(; it != end ; ++it )
	{
		CT3DTexture *pTEX = it->second->GetTexturePtr(0);

		delete pTEX;
		delete it->second;
	}

	m_mapTCUSTOMCLOAKTEX.clear();
}