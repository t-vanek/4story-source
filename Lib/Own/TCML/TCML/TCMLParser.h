#ifndef __TCML_PARSER__
#define __TCML_PARSER__

#include "cinterf.h"
#include <Afx.h>

#include <map>
#include <memory>
using namespace std;

typedef struct tagFRAMEDESC		FRAMEDESC, *LP_FRAMEDESC;
typedef struct tagCOMPINST		COMPINST, *LP_COMPINST;
typedef std::shared_ptr<const FRAMEDESC> FRAMEDESC_SHAREDPTR;
typedef std::weak_ptr<const FRAMEDESC> FRAMEDESC_WEAKPTR;

/*struct tCompBaseData
{
public:
	CString m_strTooltip;
	CString m_strText;

	DWORD m_dwID;
	BYTE m_bType;

	DWORD m_vMENU[TCML_MENU_COUNT];
	DWORD m_vImageID[TCML_IDX_COUNT];
	DWORD m_dwTooltipID;
	DWORD m_dwFontID;
	DWORD m_dwStyle;
	DWORD m_dwCOLOR;
	DWORD m_dwSND;

	int m_nMargineH;
	int m_nMargineV;
	int m_nPosX;
	int m_nPosY;
	int m_nWidth;
	int m_nHeight;

	BYTE m_bDisplay;
	BYTE m_bAlign;

public:
	tCompBaseData(
		CString strTooltip,
		CString strText,
		DWORD dwID,
		BYTE bType,
		DWORD (&vMenu)[TCML_MENU_COUNT],
		DWORD (&vImageID)[TCML_IDX_COUNT],
		DWORD dwTooltipID,
		DWORD dwFontID,
		DWORD dwStyle,
		DWORD dwCOLOR,
		DWORD dwSND,
		int nMargineH,
		int nMargineV,
		int nPosX,
		int nPosY,
		int nWidth,
		int nHeight,
		BYTE bDisplay,
		BYTE bAlign
		)
		: 
		m_strTooltip(strTooltip),
		m_strText(strText),
		m_dwID(dwID),
		m_bType(bType),
		m_dwTooltipID(dwTooltipID),
		m_dwFontID(dwFontID),
		m_dwStyle(dwStyle),
		m_dwCOLOR(dwCOLOR),
		m_dwSND(dwSND),
		m_nMargineH(nMargineH),
		m_nMargineV(nMargineV),
		m_nPosX(nPosX),
		m_nPosY(nPosY),
		m_nWidth(nWidth),
		m_nHeight(nHeight),
		m_bDisplay(bDisplay),
		m_bAlign(bAlign)
	{
		memcpy_s(m_vMENU, sizeof(m_vMENU), vMenu, sizeof(vMenu));
		memcpy_s(m_vImageID, sizeof(m_vImageID), vImageID, sizeof(vImageID));
	}

	// Due to backwards compatibility a standardctor is needed..
	tCompBaseData()
	{

	}
};*/

struct tagCOMPINST
{
public:
	CString m_strTooltip;
	CString m_strText;

	DWORD m_dwID;
	BYTE m_bType;

	TSATR m_vEX;
	DWORD m_vMENU[TCML_MENU_COUNT];
	DWORD m_vImageID[TCML_IDX_COUNT];
	DWORD m_dwTooltipID;
	DWORD m_dwFontID;
	DWORD m_dwStyle;
	DWORD m_dwCOLOR;
	DWORD m_dwSND;

	int m_nMargineH;
	int m_nMargineV;
	int m_nPosX;
	int m_nPosY;
	int m_nWidth;
	int m_nHeight;

	BYTE m_bDisplay;
	BYTE m_bAlign;
};

struct tagFRAMEDESC
{
	COMPINST m_vCOMP;

	FRAMEDESC_SHAREDPTR m_pCHILD;
	FRAMEDESC_SHAREDPTR m_pNEXT;

	tagFRAMEDESC()
	{
		m_pCHILD = nullptr;
		m_pNEXT = nullptr;
	};

	~tagFRAMEDESC()
	{

	};
};

typedef map<unsigned int, LP_TCML_LOGFONT>		FONT_MAP;
typedef map<unsigned int, LP_COMPDESC>			COMP_MAP;
typedef map<unsigned int, FRAMEDESC_SHAREDPTR>	FRAME_MAP;

#include "TCML.cpp.h"
int TCMLparse();

class TCMLParserProgress
{
public:
	virtual void OnProgress(float percent)=0;
};

class TCMLParser
{
public:
	void MoveFirstFont();
	LP_TCML_LOGFONT MoveNextFont();

	int AddLogFont(LP_TCML_LOGFONT ptr);
	LP_TCML_LOGFONT FindLogFont(unsigned int id);

	FRAMEDESC_SHAREDPTR FindFrameTemplate(unsigned int id);
	FRAMEDESC_SHAREDPTR LoadFRAME( FILE *pFILE);

	int AddParserTemplate( LP_COMPDESC ptr);

	TCMLParser();
	virtual ~TCMLParser();

	void Load( char* fname, TCMLParserProgress* pProgress=NULL);
	int Parse( char* fname );
	void Release();

	FRAME_MAP	m_Frames;
	COMP_MAP	m_Comps;
	FONT_MAP	m_Fonts;
	FONT_MAP::iterator m_it;
	BYTE m_bDeleteFont;
};

#endif
