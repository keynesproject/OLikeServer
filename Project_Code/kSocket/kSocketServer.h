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

    void ReSetCheckFd( fd_set &SelectFd );        //���s�]�w�nSelect��Socket FD;//

    int  GetMaxFdNum();                           //���o�Ҧ���FD�̪��̤j�ƭ�;//

    bool AddNewClient( SOCKET Server );           //�B�zAccept���ʧ@;//

    void RemoveClient( SOCKET Clinet );

protected:
    fd_set      m_SelectFds;                      //�ϥΫD����Select�覡�ʴ���Socket���X;//
    timeval     m_SelectTime;                     //select���߮ɶ��A�n�D����N�]0;//
    SOCKET      m_SocketServer;                   //Server�����ϥΪ�Socket;//
    sockaddr_in m_ServAddr;                       //Server��T;//

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
    //�]�wSocket�Ұʪ��ʧ@;//
    int  Active() override;

    //�����M���Ҧ��s�u��Socket;//
    void CloseAll() override;

    ClientInfo* CheckClient(sockaddr_in Addr);

private:
    //�ϥΫD����Select�覡�ʴ���Socket���X;//
    fd_set      m_ReadFds;
    timeval     m_SelectTime;

    //�إ�UDP Server��Socket�s���θ�T;//
    SOCKET      m_SocketServer;
    sockaddr_in m_ServAddr;

    std::vector< ClientInfo > m_Clients;

    void *m_ConnectWorkingClass;
    CONNECT_CALLBACK_FUNC m_ConnectCallbackFunc;
};
#endif //end of #ifndef __K_SOCKET_SERVER_H__;//

