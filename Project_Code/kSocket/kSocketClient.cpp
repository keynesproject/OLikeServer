#include "kSocketClient.h"

/////////////////////////////////////////////////////////////////////////////
// kClient                                                                     
/////////////////////////////////////////////////////////////////////////////
kClient::kClient()
: m_Client(0)
{ 
    memset( &m_ServerAddr, 0, sizeof(sockaddr_in) );
    memset( &m_LocalAddr,  0, sizeof(sockaddr_in) );

    m_SelectTime.tv_sec  = 0;
    m_SelectTime.tv_usec = 0;

    m_HandShakeTime = GetTickCount();
}

kClient::~kClient()
{
    this->SetActive( false );
}

int kClient::Create( SocketInfo Info )
{
    //設定Server端資訊;//
    m_ServerAddr.sin_family = AF_INET;                       //設定Server連線樣式;//
    m_ServerAddr.sin_port   = htons( Info.Port );            //設定Server連線port;//
    m_ServerAddr.sin_addr.s_addr = inet_addr( Info.IPV4 );   //設定Server IP ;//

    //設定本地用地址和端口;//
    m_LocalAddr.sin_family = AF_INET;                        //設定本地端連線樣式;//
    m_LocalAddr.sin_port   = htons( Info.Port );             //設定本地端連線port;//
    m_LocalAddr.sin_addr.s_addr = inet_addr( GetLocalIp() ); //設定本地端IP ;//

    return eERR_NONE;
}

int kClient::Send( char *Data, int DataLen )
{
    //傳送資訊;//
#ifdef PRINT_LOG
    printf( "[ Send ] ClientFD[%d] Send:%s \n", m_Client, Data );
#endif

    if( send( m_Client, Data, DataLen, 0 ) < 0 )
        return eERR_CLIENT_SEND;

    return eERR_NONE;
}

int kClient::Receive( char *ReData, int &ReDataLen )
{
    //這邊需要修改，因為資料有收到時會存到LIST下面，所以必須持續接收到LIST為0時才算沒有資料;//

    //if( _mRecvData.empty() )
    //{
    //    return eERR_CLIENT_RECEIVE;
    //}
    //
    //memset( ReData, 0, strlen( ReData ) );
    //memcpy( ReData, _mRecvData.begin()->Data, _mRecvData.begin()->Len );

    //_mRecvData.pop_front();

    return eERR_NONE;
}

int kClient::Select()
{
    if( m_isCloseThread == true )
    {
        pthread_exit( NULL );
        return eERR_NONE;
    }

    //寄送HandShake訊息;//
    TimeToSendHandShake();

    FD_ZERO( &m_FdRead );
    FD_SET( m_Client, &m_FdRead );

    switch( select( m_Client+1, &m_FdRead, (fd_set *)0, (fd_set *)0, &m_SelectTime ) )
    {
    case -1:
        {
#ifdef PRINT_LOG
            //printf( "[ Error ] Select Time Out!\n" );
#endif            
        }
        break;

    case 0:
        {
#ifdef PRINT_LOG
            //printf( "[ Select ] Continute! \n" );
#endif      
        }
        break;

    default:
        {
            //取得可讀的Socket;//
            if( FD_ISSET( m_Client, &m_FdRead) )
            {
                char *Buffer;
                unsigned long RecvDataSize = 0;
                if( IoctlCheck( m_Client, FIONREAD, &RecvDataSize ) == false )
                    break;

                if( RecvDataSize == 0 )
                    break;

                //讀取訊息;//
                Buffer = new char[RecvDataSize+1];
                memset( Buffer, 0, RecvDataSize+1 );
                recv( m_Client, Buffer, RecvDataSize+1, 0 );

                //查看是否為交握訊息;//
                if( CheckHandShake( Buffer, RecvDataSize ) == true )
                {
                    int ShakeLen = strlen( KSOCKET_HANDSHAKE_CODE );
                    if( ( RecvDataSize - ShakeLen ) > 0 )
                    {
                        if( m_RecvCallbackFunc != NULL )
                            m_RecvCallbackFunc( m_CallbackSocket, m_Client, Buffer+ShakeLen, RecvDataSize-ShakeLen );
                    }
                }
                else
                {
                    if( m_RecvCallbackFunc != NULL )
                        m_RecvCallbackFunc( m_CallbackSocket, m_Client, Buffer, RecvDataSize );
                }

                delete [] Buffer; 
                Buffer = NULL;
            }
        }
        break;
    }

    return eERR_NONE;
}

int kClient::Active()
{
    if( m_Client != 0 )
        CloseAll();

    //建立客戶端連接Server用Socket;//
    //_mClient = socket( AF_UNIX, SOCK_STREAM, 0 );   //本地端連線，local to host (pipes, portals);//
    m_Client = socket( AF_INET, SOCK_STREAM, 0 );     //網路連線，internetwork: UDP, TCP, etc.;//
    if( m_Client < 0 )
    {
        return eERR_OPEN_SOCKET;
    }

    //綁定端口，綁定端口後似乎會造成無法連線，原因待查，暫時註解;//
    //if( bind( _mClient, (const sockaddr *)&_mLocalAddr, sizeof( _mLocalAddr ) ) < 0 )
    //{
    //    this->CloseAll();
    //    return eERR_BIND;
    //}

    //連線Server;//
    if( connect( m_Client, (struct sockaddr *)&m_ServerAddr, sizeof( m_ServerAddr ) ) < 0)  
    {
        this->CloseAll();
        return eERR_CONNECT;
    }

    //設定非阻塞式連線;//
    if( SetNonBlocking( m_Client ) == false )
    {
        this->CloseAll();
        return eErr_SET_NONBLOCK;
    }

    return eERR_NONE;
}

void kClient::CloseAll()
{
    if( m_Client != 0 )
    {
#ifdef WIN32
        closesocket( m_Client );
#else
        close( m_Client );
#endif
        m_Client = 0;
    }
}

void kClient::TimeToSendHandShake()
{
    //計時倒數;//
    unsigned long EndTime = GetTickCount();

    if ( EndTime - m_HandShakeTime > 20000)
    {
        m_HandShakeTime = EndTime;
        // 加入要執行的函式
        this->Send( KSOCKET_HANDSHAKE_CODE, strlen(KSOCKET_HANDSHAKE_CODE) );
    }
}
