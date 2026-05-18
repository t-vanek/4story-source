#include "Stdafx.h"
#include "TClientGame.h"
#include "TClientWnd.h"
#include "Resource.h"
#include "BattleRankTex.h"

CBattleRankTex::MAPRANKTEX CBattleRankTex::m_mapRANK;

CBattleRankTex::CBattleRankTex(TImageList* pBattleRankList)
{
	m_pBattleRankList = pBattleRankList;
}

CBattleRankTex::~CBattleRankTex()
{
	Reset();
}

LPDIRECT3DTEXTURE9 CBattleRankTex::GetTexture(BYTE bRank)
{
	MAPRANKTEX::iterator finder = m_mapRANK.find(bRank);
	if (finder != m_mapRANK.end())
		return (*finder).second;

	if (!m_pBattleRankList)
		return NULL;

	const TImageList::DELEGATE_SHAREDPTR& pDelegate = m_pBattleRankList->GetDelegate();
	if (!pDelegate)
		return NULL;

	TComponent* pRankImage = pDelegate->GetImage(bRank);
	if (!pRankImage)
		return NULL;

	CD3DImage* pD3DImage = pRankImage->GetDefaultImage()->GetImage();
	if (!pD3DImage)
		return NULL;

	CTClientWnd* pMainWnd = CTClientWnd::GetInstance();
	LPDIRECT3DDEVICE9 pDevice = pMainWnd->m_Device.m_pDevice;

	LPDIRECT3DTEXTURE9 pRNDTEX;
	HRESULT hResult;

	if (bRank < 35)
	{
		hResult = D3DXCreateTexture(
			pDevice,
			(UINT)32,
			(UINT)32, 1,
			D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8,
			D3DPOOL_DEFAULT,
			&pRNDTEX);
	}
	else
	{
		hResult = D3DXCreateTexture(
			pDevice,
			(UINT)48,
			(UINT)36, 1,
			D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8,
			D3DPOOL_DEFAULT,
			&pRNDTEX);
	}

	if( FAILED(hResult) )
		return NULL;

	LPDIRECT3DSURFACE9 pBACKUPBUF;
	hResult = pDevice->GetRenderTarget(0, &pBACKUPBUF);
	if( FAILED(hResult) )
	{
		pRNDTEX->Release();
		return NULL;
	}

	LPDIRECT3DSURFACE9 pRNDBUF;
	hResult = pRNDTEX->GetSurfaceLevel( 0, &pRNDBUF);
	if( FAILED(hResult) )
	{
		pRNDTEX->Release();
		return NULL;
	}

	hResult = pDevice->SetRenderTarget(0, pRNDBUF);
	if( FAILED(hResult) )
	{
		pRNDTEX->Release();
		return NULL;
	}

	pDevice->Clear(
		0, NULL,
		D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
		D3DCOLOR_ARGB(0,0,0,0),
		0, 0 );

	RECT rc;
	rc.left = 0;
	rc.top = 0;

	if( bRank < 35 )
	{
		rc.right = (LONG) 32;
		rc.bottom = (LONG) 32;
	}
	else
	{
		rc.right = (LONG) 48;
		rc.bottom = (LONG) 36;
	}

	pD3DImage->Render(pDevice, D3DCOLOR_ARGB(255,255,255,255), 0,0, &rc);

	pDevice->SetRenderTarget(0, pBACKUPBUF);
	m_mapRANK.insert(MAPRANKTEX::value_type(bRank, pRNDTEX));

	pBACKUPBUF->Release();
	pRNDBUF->Release();
		
	return pRNDTEX;
}

void CBattleRankTex::Reset()
{
	MAPRANKTEX::iterator itr;
	MAPRANKTEX::iterator end;

	itr = m_mapRANK.begin();
	end = m_mapRANK.end();

	for(;itr!=end;itr++)
		itr->second->Release();

	m_mapRANK.clear();
}