#pragma once

class CTBoxOpenDlg : public CTClientUIBase
{
private :
	TComponent* m_pItemName;
	TComponent* m_pKey;
	TComponent* m_pEnd;
	TComponent* m_pUseless;
	TComponent* m_pUseless2;
	TComponent* m_pUseless3;

	TImageList* m_pKeyImage;
	TImageList* m_pReceived;

	BYTE m_bInvenID;
	BYTE m_bSlotID;

	DWORD m_dwTotal;

public :
	TButton* m_pOpen;
	TButton* m_pPreview;
	TGauge* m_pProgress;

	BOOL m_bShowBox;
	BOOL m_bShowList;
	
	void SetReward(WORD m_wItemID, BYTE bCount, CString strCustom = NAME_NULL);
	BOOL m_bUnboxing;
	CTBoxOpenDlg( TComponent *pParent, FRAMEDESC_SHAREDPTR pDesc );
	virtual ~CTBoxOpenDlg();
	BOOL Update(DWORD dwTickCount);
	HRESULT Render(DWORD dwTickCount);
	void Release();
	void SetSession(WORD m_wBoxID, BYTE m_bInvenID, BYTE m_bSlotID);
	void SwitchJirkus();
	void SetInvenID(BYTE bInvenID) {m_bInvenID = bInvenID;}
	void SetItemID(BYTE bItemID) {m_bSlotID = bItemID;}

	void Open();

	DWORD GetParam() const {return MAKELONG(m_bInvenID, m_bSlotID);} //make long, like my D XD
};
