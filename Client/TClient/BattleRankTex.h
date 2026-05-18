#pragma once

class CBattleRankTex
{
public:
	typedef std::map<BYTE, LPDIRECT3DTEXTURE9>	MAPRANKTEX;
public:
	static void Reset();
	LPDIRECT3DTEXTURE9 GetTexture(BYTE bRank);
private:
	TImageList* m_pBattleRankList;
	static MAPRANKTEX m_mapRANK;
public:
	CBattleRankTex(TImageList* pBattleRankList);
	virtual ~CBattleRankTex();
};
