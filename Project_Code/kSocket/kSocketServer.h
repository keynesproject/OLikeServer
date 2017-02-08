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

    void ReSetCheckFd( fd_set &SelectFd );        //���s�]�w�nSelect��Socket FD;//

    int  GetMaxFdNum();                           //���o�Ҧ���FD�̪��̤j�ƭ�;//

    bool AddNewClient( SOCKET Server );           //�B�zAccept���ʧ@;//

    ProtocolHeadStruct *Analyse( SOCKET FromSocket, char *Data );    //�ѪR�qSocket���쪺���;//

private:
    fd_set      m_SelectFds;                      //�ϥΫD����Select�覡�ʴ���Socket���X;//
    timeval     m_SelectTime;                     //select���߮ɶ��A�n�D����N�]0;//
    SOCKET      m_SocketServer;                   //Server�����ϥΪ�Socket;//
    sockaddr_in m_ServAddr;                       //Server��T;//
    std::map< SOCKET, sockaddr_in > m_Clients;    //�s�u��Client�ݸ�T;//

    void *m_ConnectWorkingClass;
    CONNECT_CALLBACK_FUNC m_ConnectCallbackFunc;
};


#endif //end of #ifndef __K_SOCKET_SERVER_H__;//

