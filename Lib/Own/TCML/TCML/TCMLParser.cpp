
#include "TCMLParser.h"
#include <stdio.h>

extern FILE *TCMLin;
extern COMPDESC* TCMLFrame;
extern LP_TCML_LOGFONT TCMLFont;
 
TCMLParser::TCMLParser()
{
    m_bDeleteFont = FALSE;
}
 
TCMLParser::~TCMLParser()
{
    Release();
}
 
void TCMLParser::Release()
{
    COMP_MAP::iterator itCOMP;
    FONT_MAP::iterator itFONT;
	
	auto it = m_Frames.begin();
    m_Frames.clear();
 
    if(m_bDeleteFont)
    {
        for( itFONT = m_Fonts.begin(); itFONT != m_Fonts.end(); itFONT++)
            delete (*itFONT).second;
        m_bDeleteFont = FALSE;
    }
 
    FiniTCMLInterface();
    m_Fonts.clear();
    m_Comps.clear();
}

void TCMLParser::Load( char* fname, TCMLParserProgress* pProgress)
{
	FILE *pFILE = nullptr;

	if (0 != fopen_s(&pFILE, fname, "rb"))
	{
#ifdef _DEBUG
		TRACE("Error has occured while opening interface file!");
#endif
		return;
	}

    int nCount = 0;
 
    fread( &nCount, sizeof(int), 1, pFILE);
    for( int i=0; i<nCount; i++)
    {
		FRAMEDESC_SHAREDPTR pFRAME = LoadFRAME(pFILE);
 
		if (pFRAME)
		{
			auto itFRAME = m_Frames.find(pFRAME->m_vCOMP.m_dwID);

			if (itFRAME == m_Frames.end())
				m_Frames.insert(std::make_pair(pFRAME->m_vCOMP.m_dwID, pFRAME));
			else
				TRACE("Frame duplication found! Id: %d\n", pFRAME->m_vCOMP.m_dwID);
		}
 
        if( pProgress )
            pProgress->OnProgress( (FLOAT) i / (FLOAT) nCount );
    }
 
    fread( &nCount, sizeof(int), 1, pFILE);
    for(int i=0; i<nCount; i++)
    {
        LP_TCML_LOGFONT pFONT = new TCML_LOGFONT();
		size_t nReadSize = sizeof(TCML_LOGFONT) - sizeof(TCML_LOGFONT*);

#ifdef _WIN64
		nReadSize -= sizeof(TCML_LOGFONT::unused);
#endif
        fread( pFONT, nReadSize, 1, pFILE);
		fseek(pFILE, 4, SEEK_CUR); // Read 4 Bytes which were marked as pointer to the next structure which doenst make any sense and just problems with x64.
		pFONT->next = nullptr;

        m_Fonts.insert( FONT_MAP::value_type( pFONT->tlfId, pFONT));
    }

    m_bDeleteFont = TRUE;
 
    fclose(pFILE);
}
 
FRAMEDESC_SHAREDPTR TCMLParser::LoadFRAME( FILE *pFILE)
{
	std::shared_ptr<FRAMEDESC> pFRAME(new FRAMEDESC());
	FRAMEDESC_SHAREDPTR *pNEXT = nullptr;
    char pBUF[MAX_TCML_SYMBOL];
    int nCount = 0;
    DWORD dwOwn = 0;
 
    fread( &pFRAME->m_vCOMP.m_dwID, sizeof(DWORD), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_bType, sizeof(BYTE), 1, pFILE);
 
    fread( pFRAME->m_vCOMP.m_vMENU, sizeof(DWORD), TCML_MENU_COUNT, pFILE); //x12
    fread( pFRAME->m_vCOMP.m_vImageID, sizeof(DWORD), 2, pFILE); //x2
    fread( &pFRAME->m_vCOMP.m_dwTooltipID, sizeof(DWORD), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_dwFontID, sizeof(DWORD), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_dwStyle, sizeof(DWORD), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_dwCOLOR, sizeof(DWORD), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_dwSND, sizeof(DWORD), 1, pFILE);
 
    fread( &pFRAME->m_vCOMP.m_nMargineH, sizeof(int), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_nMargineV, sizeof(int), 1, pFILE);
 
   // fread( &dwOwn, sizeof(DWORD), 1, pFILE);
 
	fread( &pFRAME->m_vCOMP.m_nPosX, sizeof(int), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_nPosY, sizeof(int), 1, pFILE);
	fread( &pFRAME->m_vCOMP.m_nWidth, sizeof(int), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_nHeight, sizeof(int), 1, pFILE);
	fread( &pFRAME->m_vCOMP.m_bDisplay, sizeof(BYTE), 1, pFILE);
    fread( &pFRAME->m_vCOMP.m_bAlign, sizeof(BYTE), 1, pFILE);
 
    fread( &pFRAME->m_vCOMP.m_vEX, sizeof(TSATR), 1, pFILE);
 
    fread( &nCount, sizeof(int), 1, pFILE);
    if( nCount > 0 )
    {
        fread( pBUF, sizeof(char), nCount, pFILE);
        pBUF[nCount] = '\0';
        pFRAME->m_vCOMP.m_strTooltip.Format( "%s", pBUF);
    }
 
    fread( &nCount, sizeof(int), 1, pFILE);
    if( nCount > 0 )
    {
        fread( pBUF, sizeof(char), nCount, pFILE);
        pBUF[nCount] = '\0';
        pFRAME->m_vCOMP.m_strText.Format( "%s", pBUF);
    }

    fread( &nCount, sizeof(int), 1, pFILE);
    pNEXT = &pFRAME->m_pCHILD;

	for (int i = 0; i < nCount; i++)
	{
		(*pNEXT) = LoadFRAME(pFILE);
		pNEXT = const_cast<FRAMEDESC_SHAREDPTR*>(&(*pNEXT)->m_pNEXT);
	}
 
    return pFRAME;
}
 
int TCMLParser::Parse( char* fname)
{
    int ret= -1;
 
	if (0 != fopen_s(&TCMLin, fname, "r"))
		return -1;

    if(InitTCMLInterface())
    {
        fclose(TCMLin);
        return -1;
    }
 
    ret = TCMLparse();
 
    if( 0 == ret )
    {
        LP_COMPDESC ptr1 = TCMLFrame;
        while(ptr1)
        {
            if(AddParserTemplate(ptr1))
            {
                fclose(TCMLin);
                return -2;
            }
 
            ptr1 = ptr1->next;
        }
 
        LP_TCML_LOGFONT ptr2 = TCMLFont;
        while(ptr2)
        {
            if(0!=AddLogFont(ptr2))
            {
                fclose(TCMLin);
                return -3;
            }
 
            ptr2 = ptr2->next;
        }
    }
 
    fseek( TCMLin, 0, SEEK_SET);
    fclose(TCMLin);
 
    return ret;
}
 
int TCMLParser::AddParserTemplate( LP_COMPDESC ptr)
{
    COMP_MAP::iterator finder = m_Comps.find(ptr->id);
 
    if( finder != m_Comps.end() )
        return 1;
 
    m_Comps.insert( COMP_MAP::value_type( ptr->id, ptr));
    return 0;
}
 
FRAMEDESC_SHAREDPTR TCMLParser::FindFrameTemplate( unsigned int id)
{
    FRAME_MAP::iterator finder = m_Frames.find(id);
 
    if( finder == m_Frames.end() )
        return NULL;
 
    return (*finder).second;
}
 
int TCMLParser::AddLogFont(LP_TCML_LOGFONT ptr)
{
    FONT_MAP::iterator it;
    it = m_Fonts.find(ptr->tlfId);
    if(it != m_Fonts.end()) return 1;
 
    m_Fonts.insert(FONT_MAP::value_type(ptr->tlfId, ptr));
    return 0;
}
 
LP_TCML_LOGFONT TCMLParser::FindLogFont(unsigned int id)
{
    FONT_MAP::iterator it;
    it = m_Fonts.find(id);
    if(it == m_Fonts.end())
        return NULL;
 
    return (LP_TCML_LOGFONT)(*it).second;
}
 
void TCMLParser::MoveFirstFont()
{
    m_it = m_Fonts.begin();
}
 
LP_TCML_LOGFONT TCMLParser::MoveNextFont()
{
    if(m_it != m_Fonts.end())
        return (*m_it++).second;
 
    return NULL;
}
