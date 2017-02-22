#ifndef __K_SOCKET_H__
#define __K_SOCKET_H__

#include <stdio.h>
#include <map>
#include "kProtocol.h"
#include "pthread.h"
#include "kCRC.h"

#ifdef WIN32
#include <string>
#include <typeinfo>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <nspapi.h>
#include <Windows.h> 

#else //#ifdef WIN32
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <pthread.h>
#include "fcntl.h"
#include "sys/ioctl.h"
#include <sys/time.h>
#include "sys/timeb.h"

#define SOCKET unsigned int

#endif //#ifdef WIN32

#ifndef WIN32
unsigned long GetTickCount();
#endif // !WIN32


//Client端定時會傳送這筆字串給Server，防止TimeOut;//
#define KSOCKET_HANDSHAKE_CODE "kSocketHandShark"
class kSocket;

/////////////////////////////////////////////////////////////////////////////
// Socket Type Information                                                                     
/////////////////////////////////////////////////////////////////////////////
enum SocketType
{
    eTYPE_TCP_SERVER = 0,
    eTYPE_TCP_CLIENT,
    eTYPE_UDP_SERVER
};

struct SocketInfo 
{
    SocketType Type;
    char       IPV4[16]; // XXX.XXX.XXX.XXX 最多共16byte，Client端為設定Server的IP;//
    int        Port;     // 建議範圍 1024 到 65535，Client端為設定Server的Port;//
};

enum SocketErrorMSG
{
    eERR_NONE = 0,
    eERR_CREATE,
    eERR_ACTIVE_NONE,
    eERR_OPEN_SOCKET,
    eERR_BLOCK_SETTING,
    eERR_BIND,
    eERR_LISTEN,
    eERR_ACCEPT,
    eERR_CONNECT,
    eERR_PROTOCOL,
    eErr_SET_NONBLOCK,       //10//
    
    eERR_CLIENT_CONNECT,
    eERR_CLIENT_CONNECT_TYPE,
    eERR_CLIENT_SEND,
    eERR_CLIENT_RECEIVE,
    eERR_CLIENT_SELECT,
    eERR_CLIENT_REPLY,
    eERR_SERVER_SEND,    
    eERR_SERVER_RECEIVE,
    eERR_SERVER_SELECT,                
    eERR_SERVER_REPLY,      //20//
    eERR_SERVER_SEND_NOT_FOUND_CLIEN,

    eERR_THREAD_TERMINAL,     
    eERR_RECV_EMPTY,
    eERR_SYNC_CODE,
    eERR_CRC,
    eERR_EMPTY_STATE,
    eERR_SIZE_NOT_ENOUGH,
};

/////////////////////////////////////////////////////////////////////////////
// kSocketFactory
/////////////////////////////////////////////////////////////////////////////
class kSocketFactory
{
public:
    kSocketFactory();
    ~kSocketFactory();

    kSocket *CreateSocket( SocketInfo Info );
    void CloseSocket( kSocket *Socket );
};

/////////////////////////////////////////////////////////////////////////////
// kSocket
/////////////////////////////////////////////////////////////////////////////
class kSocket
{
public:
    kSocket();
    virtual ~kSocket();

    virtual int  Create( SocketInfo Info ) = 0;
    virtual int  Send( SOCKET ToSocket, char *Data, int DataLen ) = 0;
    virtual int  Receive( char *ReData, int &ReDataLen ) = 0;
    virtual int  Select() = 0;

    //檢查是否為HandShake訊息;//
    bool CheckHandShake( char *Data, int DataSize );

    //接收到資料的CallbackFunction，參數說明:1作用的kSocket類別, 2作用的Socket, 3接收的檔案, 4接收檔案大小;//
    typedef int (*RECV_CALLBACK_FUNC)( void*, SOCKET, char*, int );
    void SetReceiveCallBack( void *WorkingClass, RECV_CALLBACK_FUNC Func );

    bool SetActive( bool IsActive );

    bool IsCloseThread();

    bool CheckSyncCode( char *Data );       //驗證同步碼;//
    int  GetProtocolStructSize( char Protocol );
    bool CheckCRC( ProtocolHeadStruct *CheckData );

    const char* GetLocalIp();

protected:
    virtual int  Active();                  //設定Socket啟動的動作;//
    virtual void CloseAll() = 0;            //關閉清除所有連線的Socket;//

    bool SetNonBlocking( SOCKET &Socket );
            
protected:
    void *m_CallbackSocket;
    RECV_CALLBACK_FUNC m_RecvCallbackFunc;

    bool m_isCloseThread;

    pthread_t m_Thread;

    char m_LocalIp[16];
};

#endif //__K_SOCKET_H__

