#ifndef __K_SOCKET_SERVER_H__
#define __K_SOCKET_SERVER_H__

#include <vector>
#include "kSocket.h"

typedef int(*CONNECT_CALLBACK_FUNC)(void* WorkingClass, SOCKET FromSocket, bool IsConnect, char *Ip);

struct ClientInfo
{
    SOCKET        Socket;
    sockaddr_in   Addr;
#ifdef WIN32
    unsigned long TimeOut;
#else
    timeval TimeOut;
#endif
};

/////////////////////////////////////////////////////////////////////////////
//  kTcpServer                                                                    
/////////////////////////////////////////////////////////////////////////////
class kTcpServer : public kSocket
{
public:
    kTcpServer();
    ~kTcpServer();

    int  Create( SocketInfo Info )  override;
    int  Send( SOCKET ToSocket, char *Data, int DataLen ) override;
    int  Receive( char *ReData, int &ReDataLen )  override;
    int  Select()  override;
        
    void SetClientConnectCallBack( void *WorkingClass, CONNECT_CALLBACK_FUNC Func );

protected:
    int  Active() override;
    void CloseAll()  override;
    void Close( SOCKET Fd );

    void ReSetCheckFd( fd_set &SelectFd );        //重新設定要Select的Socket FD;//

    int  GetMaxFdNum();                           //取得所有的FD裡的最大數值;//

    bool AddNewClient( SOCKET Server );           //處理Accept的動作;//

    void RemoveClient( SOCKET Clinet );

protected:
    fd_set      m_SelectFds;                      //使用非阻塞Select方式監測的Socket集合;//
    timeval     m_SelectTime;                     //select輪詢時間，要非阻塞就設0;//
    SOCKET      m_SocketServer;                   //Server本身使用的Socket;//
    sockaddr_in m_ServAddr;                       //Server資訊;//

    std::vector< ClientInfo > m_Clients;

    void *m_ConnectWorkingClass;
    CONNECT_CALLBACK_FUNC m_ConnectCallbackFunc;
};

/////////////////////////////////////////////////////////////////////////////
//  kUdpServer                                                                    
/////////////////////////////////////////////////////////////////////////////
class kUdpServer : public kSocket
{
public:
    kUdpServer();
    ~kUdpServer();

    int  Create(SocketInfo Info) override;
    int  Send( SOCKET ToSocket, char *Data, int DataLen) override;
    int  Receive(char *ReData, int &ReDataLen) override;
    int  Select() override;

    SOCKET GetSocketFd();

    void SetClientConnectCallBack(void *WorkingClass, CONNECT_CALLBACK_FUNC Func);

private:
    //設定Socket啟動的動作;//
    int  Active() override;

    //關閉清除所有連線的Socket;//
    void CloseAll() override;

    ClientInfo* CheckClient(sockaddr_in Addr);

private:
    //使用非阻塞Select方式監測的Socket集合;//
    fd_set      m_ReadFds;
    timeval     m_SelectTime;

    //建立UDP Server的Socket編號及資訊;//
    SOCKET      m_SocketServer;
    sockaddr_in m_ServAddr;

    std::vector< ClientInfo > m_Clients;

    void *m_ConnectWorkingClass;
    CONNECT_CALLBACK_FUNC m_ConnectCallbackFunc;
};
#endif //end of #ifndef __K_SOCKET_SERVER_H__;//

