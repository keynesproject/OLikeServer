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
    //�]�wServer�ݸ�T;//
    m_ServerAddr.sin_family = AF_INET;                       //�]�wServer�s�u�˦�;//
    m_ServerAddr.sin_port   = htons( Info.Port );            //�]�wServer�s�uport;//
    m_ServerAddr.sin_addr.s_addr = inet_addr( Info.IPV4 );   //�]�wServer IP ;//

    //�]�w���a�Φa�}�M�ݤf;//
    m_LocalAddr.sin_family = AF_INET;                        //�]�w���a�ݳs�u�˦�;//
    m_LocalAddr.sin_port   = htons( Info.Port );             //�]�w���a�ݳs�uport;//
    m_LocalAddr.sin_addr.s_addr = inet_addr( GetLocalIp() ); //�]�w���a��IP ;//

    return eERR_NONE;
}

int kClient::Send( char *Data, int DataLen )
{
    //�ǰe��T;//
#ifdef PRINT_LOG
    printf( "[ Send ] ClientFD[%d] Send:%s \n", m_Client, Data );
#endif

    if( send( m_Client, Data, DataLen, 0 ) < 0 )
        return eERR_CLIENT_SEND;

    return eERR_NONE;
}

int kClient::Receive( char *ReData, int &ReDataLen )
{
    //�o��ݭn�ק�A�]����Ʀ�����ɷ|�s��LIST�U���A�ҥH�������򱵦���LIST��0�ɤ~��S�����;//

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

    //�H�eHandShake�T��;//
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
            //���o�iŪ��Socket;//
            if( FD_ISSET( m_Client, &m_FdRead) )
            {
                char *Buffer;
                unsigned long RecvDataSize = 0;
                if( IoctlCheck( m_Client, FIONREAD, &RecvDataSize ) == false )
                    break;

                if( RecvDataSize == 0 )
                    break;

                //Ū���T��;//
                Buffer = new char[RecvDataSize+1];
                memset( Buffer, 0, RecvDataSize+1 );
                recv( m_Client, Buffer, RecvDataSize+1, 0 );

                //�d�ݬO�_���洤�T��;//
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

    //�إ߫Ȥ�ݳs��Server��Socket;//
    //_mClient = socket( AF_UNIX, SOCK_STREAM, 0 );   //���a�ݳs�u�Alocal to host (pipes, portals);//
    m_Client = socket( AF_INET, SOCK_STREAM, 0 );     //�����s�u�Ainternetwork: UDP, TCP, etc.;//
    if( m_Client < 0 )
    {
        return eERR_OPEN_SOCKET;
    }

    //�j�w�ݤf�A�j�w�ݤf����G�|�y���L�k�s�u�A��]�ݬd�A�Ȯɵ���;//
    //if( bind( _mClient, (const sockaddr *)&_mLocalAddr, sizeof( _mLocalAddr ) ) < 0 )
    //{
    //    this->CloseAll();
    //    return eERR_BIND;
    //}

    //�s�uServer;//
    if( connect( m_Client, (struct sockaddr *)&m_ServerAddr, sizeof( m_ServerAddr ) ) < 0)  
    {
        this->CloseAll();
        return eERR_CONNECT;
    }

    //�]�w�D���릡�s�u;//
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
    //�p�ɭ˼�;//
    unsigned long EndTime = GetTickCount();

    if ( EndTime - m_HandShakeTime > 20000)
    {
        m_HandShakeTime = EndTime;
        // �[�J�n���檺�禡
        this->Send( KSOCKET_HANDSHAKE_CODE, strlen(KSOCKET_HANDSHAKE_CODE) );
    }
}
