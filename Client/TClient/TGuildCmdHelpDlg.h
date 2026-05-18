#pragma once

class CTGuildCmdHelpDlg : public CTClientUIBase
{
protected:
	BOOL m_bNeedInit;
	TList* m_pList;

public:
	void Init();

public:
	virtual void ShowComponent( BOOL bVisible = TRUE);

public:
	CTGuildCmdHelpDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc);
	virtual ~CTGuildCmdHelpDlg();
};