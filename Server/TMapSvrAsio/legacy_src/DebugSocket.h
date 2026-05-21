class CDebugSocket 
{
public:
	CDebugSocket(CString strServiceName);
	~CDebugSocket();

private: 
	SOCKET				m_SendSock;
	SOCKADDR_IN			m_siAddr;
	CRITICAL_SECTION	m_DebugLock;
	CString				m_strServiceName;

	VOID CLOSESOCKET( SOCKET &x )	{	if(x != INVALID_SOCKET) closesocket(x), x = INVALID_SOCKET; }

public:
	BOOL Initialize(char *szIPAddr, int nPort);
	void LogLogin(char* szIP, CString strName);
	void LogInvalidPacket(char* szIP);
	void LogNonVaildPacket(char* szIP, WORD wPacketID);
	void LogString(const char* pszStr, ...);

private:
	void SendToTerminal(char *szData);

};