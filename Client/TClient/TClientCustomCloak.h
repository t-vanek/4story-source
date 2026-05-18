#pragma once

class CTClientCustomCloakTexture
{
public:
	LPDIRECT3DTEXTURE9 m_pSYMBOL;
	LPDIRECT3DTEXTURE9 m_pMANTLE;

	CTClientCustomCloakTexture()
		: m_pSYMBOL(NULL), m_pMANTLE(NULL) {}
};

class CTClientCustomCloak;

typedef std::map< WORD, LPTEXTURESET> MAPTCUSTOMCLOAKTEX, *LPMAPTCUSTOMCLOAKTEX;
class CTClientCustomCloak
{
public:
	

protected:
	static DWORD m_dwLEFTTICK;
public:
	

protected:
	

public:
	static BOOL OnDeviceReset( BOOL bRelease );
	

	static void SetMantleTexture( CTClientChar*, CTClientCustomCloak* pMark );

	
	
	
	static MAPTCUSTOMCLOAKTEX m_mapTCUSTOMCLOAKTEX;
	static LPTEXTURESET GetMantleTexture( WORD wID);
	static void ReleaseTMantleBakingTexture();

protected:
	WORD m_wID;

public:
	CTClientCustomCloak(WORD wID);

	CTClientCustomCloak(const CTClientCustomCloak& r);
	
	bool operator < ( const CTClientCustomCloak& r) const;
	CTClientCustomCloak& operator = ( const CTClientCustomCloak& r);


	void SetTexID(WORD wID)				{ m_wID = wID; }
	
	WORD GetTexID() const			{ return m_wID; }
};