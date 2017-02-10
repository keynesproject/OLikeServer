#include "kSocket.h"
#include "kSocketServer.h"
#include "kSocketClient.h"

#ifndef WIN32
unsigned long GetTickCount()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
#endif // !WIN32

/////////////////////////////////////////////////////////////////////////////
// Socket 的 Select 功能由thread去跑
/////////////////////////////////////////////////////////////////////////////
void *ThreadSelect( void *Arg )
{
    if( Arg == 0 )
        return NULL;

    pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

    while ( 1 )
    {
        ( (kSocket*)Arg )->Select();

#ifdef WIN32
        Sleep( 2 );
#else
        usleep( 2000 );
#endif
    } 

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// kSocketFactory                                                                    
/////////////////////////////////////////////////////////////////////////////
//計數Socket的使用量;//
static int NetStarteed = 0;

kSocketFactory::kSocketFactory()
{

}

kSocketFactory::~kSocketFactory()
{

}

kSocket *kSocketFactory::CreateSocket( SocketInfo Info )
{
#ifdef WIN32
    /* Start up the windows networking */
    if( NetStarteed <= 0 )
    {
        WORD VersionWanted = MAKEWORD(2,0);
        WSADATA wsaData;
        if ( WSAStartup( VersionWanted, &wsaData ) != 0 ) 
            return NULL;
    }
#endif

    kSocket *NewSocket = NULL;
    if( Info.Type == eTYPE_SERVER )
    {
        NewSocket = new kServer();
    }
    else
    {
        NewSocket = new kClient();
    }

    int Error = NewSocket->Create( Info );
    if( Error != 0 )
    {
        printf( "Socket Create Error : %d\n", Error );
        delete NewSocket;
        NewSocket = NULL;
        return NULL;
    }

    NetStarteed++;
    return NewSocket;
}

void kSocketFactory::CloseSocket( kSocket *Socket )
{
    if( Socket == NULL )
        return;

    delete Socket;
    Socket = NULL;
    NetStarteed--;

#ifdef WIN32
    if( NetStarteed <= 0 )
        WSACleanup();
#endif
}

/////////////////////////////////////////////////////////////////////////////
// kSocket                                                                     
/////////////////////////////////////////////////////////////////////////////
kSocket::kSocket()
{
    memset( &m_Thread, 0, sizeof(pthread_t) );
    m_isCloseThread = false;

    m_RecvCallbackFunc = NULL;
}

kSocket::~kSocket()
{

}

bool kSocket::CheckHandShake( char *Data, int DataSize )
{
    if( DataSize < (int)strlen(KSOCKET_HANDSHAKE_CODE) )
        return false;

    if( memcmp( Data, KSOCKET_HANDSHAKE_CODE, strlen(KSOCKET_HANDSHAKE_CODE) ) == 0 )
        return true;

    return false;
}

void kSocket::SetReceiveCallBack( void *WorkingClass, RECV_CALLBACK_FUNC Func )
{
    if( WorkingClass == NULL || Func == NULL )
        return;

    m_CallbackSocket = WorkingClass;
    m_RecvCallbackFunc = Func;
}

bool kSocket::IsCloseThread()
{
    return m_isCloseThread;
}

bool kSocket::SetActive( bool IsActive )
{
    if( IsActive == true )
    {
        //向Server連線;//
        int Error = this->Active();
        if( Error != eERR_NONE )
        {
            printf( "@ failure @ Active Error : %d \n", Error );
            return false;
        }

        //開啟thread;//
        m_isCloseThread = false;
        //pthread_attr_t attr;
        //pthread_attr_init (&attr);
        //pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
        pthread_create( &m_Thread, NULL, ThreadSelect, this );
    }
    else
    {
        //關閉thread;//
#ifdef WIN32
        if( m_Thread.p != NULL )
#else
        if(m_Thread != 0 )
#endif // WIN32
        {
            pthread_cancel( m_Thread );

            m_isCloseThread = true;

            void *Status = NULL;
            pthread_join( m_Thread, &Status );
            if ( Status != PTHREAD_CANCELED )
                return false;

            memset( &m_Thread, 0, sizeof(pthread_t) );
        }

        //關閉Socket;//
        this->CloseAll();
    }

    return true;
}

int kSocket::Active()
{
    return eERR_ACTIVE_NONE;
}

bool kSocket::CheckSyncCode( char *Data )
{
    char SyncCode[] = { 0X4F,0X50,0X45 };
    if( memcmp( Data, SyncCode, 3 ) == 0 )
        return true;
    
    return false;
}

bool kSocket::SetNonBlocking( SOCKET &Socket )
{
#ifdef WIN32
    unsigned long mode = 1;
    ioctlsocket( Socket, FIONBIO, &mode);
#else
    if( fcntl( Socket, F_SETFL, O_NONBLOCK ) < 0 )
        return false;
#endif

    return true;
}

const char *kSocket::GetLocalIp()
{
    memset( m_LocalIp, 0, 16 );

#ifdef WIN32

    //獲得本機主機名;//
    char ClientName[256];
    gethostname( ClientName, sizeof(ClientName));

    //根據本機主機名得到本機ip;//
    hostent *Host;
    Host = gethostbyname( ClientName );

    //把ip換成字符串形式;//
    const char* strIPAddr = inet_ntoa( *(struct in_addr*)Host->h_addr_list[0] );

    strcpy_s( m_LocalIp, strIPAddr );

#else
    struct ifaddrs  *ifc, *ifc1;
    char            IpAddr[64];
    char            NetMask[64];

    if( getifaddrs( &ifc ) == 0 )
    {
        ifc1 = ifc;

        printf( "Iface\tIP address\tNetmask\n" );

        for( ; ifc != NULL; ifc = (*ifc).ifa_next )
        {
            printf( "%s", (*ifc).ifa_name );

            if( NULL != (*ifc).ifa_addr )
            {
                inet_ntop( AF_INET, &(((struct sockaddr_in*)((*ifc).ifa_addr))->sin_addr), IpAddr, 64 );
                printf( "\t%s", IpAddr );
                sprintf( m_LocalIp, "%s", IpAddr );
            }
            else
            {
                sprintf( m_LocalIp, "XXX.XXX.XXX.XXX" );
                printf("\t\t");
            }

            if( NULL != (*ifc).ifa_netmask ) 
            {
                inet_ntop( AF_INET, &(((struct sockaddr_in*)((*ifc).ifa_netmask))->sin_addr), NetMask, 64 );
                printf( "\t%s", NetMask );
            }
            else
            {
                printf("\t\t");
            }
            printf("\n");
        }

        freeifaddrs( ifc1 );
    }
    else
    {
        sprintf( m_LocalIp, "XXX.XXX.XXX.XXX" );
    }
#endif    

    return m_LocalIp;
}

bool kSocket::CheckCRC( ProtocolHeadStruct *CheckData )
{
    //CRC驗證，Client及Srver在計算資料結構的CRC時都是以CRC=0去計算的;//
    //所以以下要先把CRC設為0再去計算結果才為正確的值;//
    unsigned int FromCrc = CheckData->CRC;
    CheckData->CRC = 0;
    unsigned int CRC = CalCrcByte( (unsigned char*)CheckData, CheckData->Length );
    
    //還原CRC值;//
    CheckData->CRC = FromCrc;

    if( CRC != FromCrc )
        return false;

    return true;
}

int kSocket::GetProtocolStructSize( char Protocol )
{
    switch (Protocol)
    {
    case PROTOCOL_S_REPLY:
    case PROTOCOL_C_REPLY:
        return sizeof(P_Reply);

    case PROTOCOL_C_CONNECT:
        return sizeof(P_C_Request);

    case PROTOCOL_S_RESPONSE:
        return sizeof(P_S_Response);
    }

    return 0;
}

