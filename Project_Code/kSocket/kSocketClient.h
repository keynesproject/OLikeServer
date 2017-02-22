#ifndef __K_SOCKET_CLIENT_H__ 
#define __K_SOCKET_CLIENT_H__

#include <list>
#include "kSocket.h"

//#define PRINT_LOG

/////////////////////////////////////////////////////////////////////////////
// kTcpClient                                                                     
/////////////////////////////////////////////////////////////////////////////
class kTcpClient : public kSocket
{
public:
    kTcpClient();
    ~kTcpClient();

    int  Create( SocketInfo Info );
    int  Send( SOCKET TargetSocket, char *Data, int DataLen );
    int  Receive( char *ReData, int &ReDataLen );
    int  Select();

private:
    int  Active();
    void CloseAll();   

    ProtocolHeadStruct* Analyse( char *Data ); //解析從Socket收到的資料;//

    void TimeToSendHandShake();

private:
    SOCKET  m_Client;                    //Client與Server通訊所使用的Socket;//
    fd_set  m_FdRead;
    timeval m_SelectTime;

    sockaddr_in m_ServerAddr;
    sockaddr_in m_LocalAddr;

    std::list< ProtocolHeadStruct* > m_RecvData;

    unsigned long m_HandShakeTime;    
};



#endif //end of #ifndef __K_SOCKET_CLIENT_H__ ;//

