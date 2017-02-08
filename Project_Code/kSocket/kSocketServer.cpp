#include "kSocketServer.h"

/////////////////////////////////////////////////////////////////////////////
// kServer                                                                     
/////////////////////////////////////////////////////////////////////////////
kServer::kServer()
: m_SocketServer(0)
, m_ConnectWorkingClass( NULL )
, m_ConnectCallbackFunc( NULL )
{
    m_SelectTime.tv_sec = 0;
    m_SelectTime.tv_usec = 0;
}

kServer::~kServer()
{
    this->CloseAll();
}

int kServer::Create( SocketInfo Info )
{
    //_mSocket = socket( AF_UNIX, SOCK_STREAM, 0 );   //���a�ݳs�u�Alocal to host (pipes, portals);//
    m_SocketServer = socket( AF_INET, SOCK_STREAM, 0 );     //�����s�u�Ainternetwork: UDP, TCP, etc.;//
    if( m_SocketServer < 0 )
        return eERR_OPEN_SOCKET;

    //�D���릡�q�T�]�w;//
    if( SetNonBlocking( m_SocketServer ) == false )
        return eERR_BLOCK_SETTING;

    //�]�wServer��T;//
    memset( (char *)&m_ServAddr, 0, sizeof( m_ServAddr ) );
    m_ServAddr.sin_family      = AF_INET;                 //�]�w�s�u�˦�;//
    m_ServAddr.sin_port        = htons( Info.Port );      //�]�w�s�uPORT���X;//
    m_ServAddr.sin_addr.s_addr = htonl( INADDR_ANY );     //�]�w���A�������Ӫv���󤶭��W���s���ШD;//
    
    kSocket::GetLocalIp();

    return eERR_NONE;
}

int kServer::Active()
{
    //Bind;//
    if( bind( m_SocketServer, (struct sockaddr *)&m_ServAddr, sizeof(m_ServAddr) ) < 0 )
    {
        this->CloseAll();
        return eERR_BIND;
    }

    //Listen,�ĤG�ӰѼƬ����\�i�J���s���ơA�j�h�ƨt�Τ��\�̤j�Ʀr��20;//
    if( listen( m_SocketServer, 1000 ) < 0 )
    {
        this->CloseAll();
        return eERR_LISTEN;
    }

    printf( "[ StartUp ] Waiting client Connect!! \n" );

    return eERR_NONE;
}

int kServer::Send( char *Data, int DataLen )
{
    //�̷�protocol���e�H�e�����w��Socket;//
    //if( send( m_SocketServer, Data, DataLen, 0 ) < 0 )
    //{
    //    return eERR_SERVER_SEND;
    //}

    return eERR_NONE;
}

int kServer::Receive( char *ReData, int &ReDataLen )
{    
    return eERR_NONE;
}

int kServer::Select()
{
    if( m_isCloseThread == true )
    {
        pthread_exit( NULL );
        return eERR_NONE;
    }

    //���s�]�w�nSelect���l��;//
    ReSetCheckFd( m_SelectFds );

    switch( select( this->GetMaxFdNum()+1, &m_SelectFds, (fd_set *)0, (fd_set *)0, &m_SelectTime ) )
    {
    case -1:
        {
            //printf( "[ Error ] Select Time Out!\n" );
        }
        break;

    case 0:
        {
            //printf( "[ Select ] Continute! \n" );
        }
        break;

    default:
        {
            //���Server Socket��client�ШD�s�u���T��;//
            if( FD_ISSET( m_SocketServer, &m_SelectFds ) )
            {
                //�[�J�s��Client�s�u��T;//
                AddNewClient( m_SocketServer );
            }
            //���Client�ݪ�Socket����L���ШD;//
            else
            {
                //�o�̭n�y�L�`�N�@�UMAP��Erase���g�k�O���F��PLinux�U��MAP�@�ΦP�˵{���X�Ӽg��;//
                std::map< SOCKET, sockaddr_in >::iterator it = m_Clients.begin();
                for( ; it != m_Clients.end(); )
                {                        
                    if( FD_ISSET( it->first, &m_SelectFds ) )
                    {
                        unsigned long ReceiveDataSize;

                        if( IoctlCheck( it->first, FIONREAD, &ReceiveDataSize ) == false )
                            return eERR_SERVER_RECEIVE;

                        if( ReceiveDataSize == 0 )
                        {
                            printf( "\n" );
                            printf( "[! Removing !] \n" );
                            printf( "  Remove client on Fd %d \n", it->first );
                            printf( "  Total Clients : %d \n", m_Clients.size()-1 );

                            if( m_ConnectCallbackFunc != NULL )
                                m_ConnectCallbackFunc( m_ConnectWorkingClass, it->first, false, NULL );

                            Close( it->first );
                            FD_CLR( it->first, &m_SelectFds );
                            m_Clients.erase( it++ );
                        }
                        else
                        {
                            char *Buffer = new char[ReceiveDataSize];
                            memset( Buffer, 0, ReceiveDataSize );
                            recv( it->first, Buffer, ReceiveDataSize, 0 );

                            //�d�ݬO�_���洤�T��;//
                            if( CheckHandShake( Buffer, ReceiveDataSize ) == true )
                            {
                                int ShakeLen = strlen( KSOCKET_HANDSHAKE_CODE );
                                if( ( ReceiveDataSize - ShakeLen ) > 0 )
                                {
                                    if( m_RecvCallbackFunc != NULL )
                                        m_RecvCallbackFunc( m_CallbackSocket, it->first, Buffer+ShakeLen, ReceiveDataSize-ShakeLen );
                                }
                            }
                            else
                            {
                                if( m_RecvCallbackFunc != NULL )
                                    m_RecvCallbackFunc( m_CallbackSocket, it->first, Buffer, ReceiveDataSize );
                            }
                            delete [] Buffer;
                            Buffer = NULL;

                            it++;
                        }
                    }
                    else
                    {
                        it++;
                    }
                }
            }
        }
        break;
    }

    return eERR_NONE;
}

void kServer::SetClientConnectCallBack( void *WorkingClass, CONNECT_CALLBACK_FUNC Func )
{
    if( WorkingClass == NULL || Func == NULL )
        return;

    m_ConnectWorkingClass = WorkingClass;
    m_ConnectCallbackFunc = Func;
}

void kServer::ReSetCheckFd( fd_set &SelectFd )
{
    FD_ZERO( &SelectFd );
    FD_SET( m_SocketServer, &SelectFd );

    std::map< SOCKET, sockaddr_in >::iterator it = m_Clients.begin();
    for( ; it != m_Clients.end(); it++ )
    {
        FD_SET( it->first, &SelectFd );
    }
}

int kServer::GetMaxFdNum()
{
    unsigned int MaxNum = m_SocketServer;
    std::map< SOCKET, sockaddr_in >::iterator it = m_Clients.begin();
    for( ; it != m_Clients.end(); it++ )
    {
        if( it->first > MaxNum )
            MaxNum = it->first;
    }

    return MaxNum;
}

bool kServer::AddNewClient( SOCKET Server )
{
    //Accept;//
    struct sockaddr_in ClientAddr;
    socklen_t ClientLen = sizeof( ClientAddr );
    SOCKET ClientSocket = accept( Server, (struct sockaddr *) &ClientAddr, &ClientLen );
    if( ClientSocket < 0 )
        return false;

    m_Clients.insert( std::make_pair( ClientSocket, ClientAddr ) );

    char IP[17];
    sprintf_s( IP, "%d.%d.%d.%d",                
                   ClientAddr.sin_addr.S_un.S_un_b.s_b1,
                   ClientAddr.sin_addr.S_un.S_un_b.s_b2,
                   ClientAddr.sin_addr.S_un.S_un_b.s_b3,
                   ClientAddr.sin_addr.S_un.S_un_b.s_b4 );

    //�T��;//
    printf( "\n" );
    printf( "[ Accept ]\n" );
    printf( "  Fd No : %d\n", ClientSocket );
    printf( "  Client Port No : %d \n", ClientAddr.sin_port );
    printf( "  Client Address : %s \n", IP );
    printf( "  Total Clients : %d \n", m_Clients.size() );

    if( m_ConnectCallbackFunc != NULL )
        m_ConnectCallbackFunc( m_ConnectWorkingClass, ClientSocket, true, IP );

    return true;
}

void kServer::Close( SOCKET Fd )
{
#ifdef WIN32
    closesocket( Fd );
#else
    close( Fd );
#endif
}

void kServer::CloseAll()
{    
    if( m_SocketServer != 0 )
    {
        Close( m_SocketServer );
        m_SocketServer = 0;
    }

    std::map< SOCKET, sockaddr_in >::iterator it = m_Clients.begin();
    for( ; it != m_Clients.end(); it++ )
    {
        Close( it->first );
    }
    m_Clients.clear();
}


