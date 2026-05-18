#pragma once

class CTSkygardenEnd : public CTClientUIBase
{
private :

	TImageList*		m_pWinList;


public :

	CTSkygardenEnd( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTSkygardenEnd();

	void ShowInterface(WORD wCountry);
};
