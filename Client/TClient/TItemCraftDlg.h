#pragma once

struct CRAFTMATERIAL
{
	DWORD m_dwItemID;
	WORD m_wCount;
};

typedef std::vector<CRAFTMATERIAL>	VTCRAFTMATERIAL, *LPVTCRAFTMATERIAL;

struct CRAFTREWARD
{
	WORD m_wID;
	BYTE m_bRewardBtn;
	DWORD m_dwItemID;
	BYTE m_bCategory;
	VTCRAFTMATERIAL m_vMaterial;
};

class CTItemCraftDlg : public CTClientUIBase
{
protected:
	TButton*		m_pCreateBtn;			//CreateBtn Rechts Unten --> "Erschaffen" ID = 27570 | 6BB2
	

	TComponent*		m_pDlgTitle;			//DialogTitel Itemherstellung -> Item | 0013
	TComponent*		m_pListTitle;			//Listentitel "Liste" ID = 27573   | 6BB5
	TComponent*		m_pNeededTitle;			//Titel für die Gegenstände die benötigt werden "Begriff" <-- Deutsch Translate WTF?! ID = 26090 | 65EA
	TComponent*		m_pNeededDesTitle;		//Titel der Beschreibung des Gegenstandes das benötigt wird "Zusammenfassung" ID = 27583 | 6BBF

	TButton*		m_pCatBtn[255];			//Kategorie Button  ID = 27927 | 6D17
	TButton*		m_pCatBtnPressed[255];	//Depressiver Button ID = 27928 | 6D18

	TList*			m_pMateriallistTitle;	//Materialliste Item Name ID = 27581 Count = ID(27581) |6BBD
	TList*			m_pMateriallistCount;	//Materialliste Item Count (in Besitz)/(Wie viel gebraucht wird) ID = 27582 | 6BBE

	TComponent*		m_pRewardItem[255];		//Linke Liste aufklappbar per Kategoriebutton | Das Rewarditem steht darin. ID = 27578 | 6BBA
	TComponent*		m_pRewardArrow[255];	//Pfeil vor der Liste
	TButton*		m_pRewardBtn[255];		//Button der Rewardliste

	TComponent*		m_pRewardTitle;			//Titel des Gegenstandes das erstellt wird "Ergebnis" ID = 27586
	TImageList*		m_pRewardIcon;			//Liste für die Icons des erstellten Gegenstandes ID = 27588
	TComponent*		m_pRewardItemTitle;		//Name des Gegenstandes ID = 27589;
	


private :
	std::vector<CRAFTREWARD> m_vCraftReward;

	DWORD CharItemCount( DWORD dwItemID );

	BYTE m_bCategoryCount;
	BYTE m_bRewardCount;
	BYTE m_bSelectedCat;
	BYTE m_bMaterialCount;

public :

	void AddRewardItem(WORD wID, BYTE bCat, DWORD dwItemID );
	void AddMaterial(DWORD dwItemID, WORD wCount );
	void AddCategory( CString strName ) ;

	CTItemCraftDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTItemCraftDlg();

	HRESULT Render(DWORD dwTickCount);
};
