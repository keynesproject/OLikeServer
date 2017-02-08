#ifndef __K_SOCKET_SERVER_H__
#define __K_SOCKET_SERVER_H__

#include <list>
#include "kSocket.h"


/////////////////////////////////////////////////////////////////////////////
//  kServer                                                                    
/////////////////////////////////////////////////////////////////////////////
class kServer : public kSocket
{
public:
    kServer();
    ~kServer();

    int  Create( SocketInfo Info );
    int  Send( char *Data, int DataLen );
    int  Receive( char *ReData, int &ReDataLen );
    int  Select();

    typedef int (*CONNECT_CALLBACK_FUNC)( void* WorkingClass, SOCKET FromSocket, bool IsConnect, char *Ip );
    void SetClientConnectCallBack( void *WorkingClass, CONNECT_CALLBACK_FUNC Func );

private:
    int  Active();
    void CloseAll();
    void Close( SOCKET Fd );

    void ReSetCheckFd( fd_set &SelectFd );        //重新設定要Select的Socket FD;//

    int  GetMaxFdNum();                           //取得所有的FD裡的最大數值;//

    bool AddNewClient( SOCKET Server );           //處理Accept的動作;//

    ProtocolHeadStruct *Analyse( SOCKET FromSocket, char *Data );    //解析從Socket收到的資料;//

private:
    fd_set      m_SelectFds;                      //使用非阻塞Select方式監測的Socket集合;//
    timeval     m_SelectTime;                     //select輪詢時間，要非阻塞就設0;//
    SOCKET      m_SocketServer;                   //Server本身使用的Socket;//
    sockaddr_in m_ServAddr;                       //Server資訊;//
    std::map< SOCKET, sockaddr_in > m_Clients;    //連線的Client端資訊;//

    void *m_ConnectWorkingClass;
    CONNECT_CALLBACK_FUNC m_ConnectCallbackFunc;
};


#endif //end of #ifndef __K_SOCKET_SERVER_H__;//

