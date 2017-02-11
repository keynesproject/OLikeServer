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
    int Yes = 1;
    setsockopt( m_SocketServer, SOL_SOCKET, SO_REUSEADDR, (const char*)&Yes, sizeof(int));
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
    if (m_isCloseThread == true)
    {
        pthread_exit(NULL);
        return eERR_NONE;
    }

    //���s�]�w�nSelect���l��;//
    ReSetCheckFd(m_SelectFds);
    fd_set ReadFds = m_SelectFds;

    int Res = select(this->GetMaxFdNum() + 1, &ReadFds, (fd_set *)0, (fd_set *)0, &m_SelectTime);

    //select���~,�򥻤W��Timeout���~;//
    if (Res == -1)
    {
        //printf("[ Error ] Select error Timeout!\n");
        return eERR_SERVER_SELECT;
    }

    //��ܨS���ݭn�@�Ϊ� SOCKET;//
    if (Res == 0)
    {
        //printf("[ Select ] Continute! \n");
        //�P�_Client Socket�O�_�L�[�S�ǿ�T��,�n�����s�u;//
        /*
        std::vector< ClientInfo >::iterator it = m_Clients.begin();
        for (; it != m_Clients.end(); )
        {
#ifdef WIN32
            unsigned long NowTime = GetTickCount();
            if (NowTime - it->TimeOut > 200000)
#else
            timeval NowTime;
            gettimeofday(&NowTime, NULL);
            if (NowTime.tv_sec - it->TimeOut.tv_sec >= 20)
#endif                  
            {
                //�L20��S���ǿ�L���,�⦹Client�����_�u;//
                RemoveClient(it->Socket);

                it = m_Clients.erase(it);
            }
            else
            {
                it++;
            }
        }
        */
        return eERR_NONE;
    }


    //���Server Socket��client�ШD�s�u���T��;//
    if (FD_ISSET(m_SocketServer, &ReadFds))
    {
        //�[�J�s��Client�s�u��T;//
        AddNewClient(m_SocketServer);
    }
    //���Client�ݪ�Socket����L���ШD;//
    else
    {
        std::vector< ClientInfo >::iterator it = m_Clients.begin();
        for (; it != m_Clients.end(); )
        {
            if (FD_ISSET(it->Socket, &ReadFds))
            {
#ifdef WIN32
                unsigned long ReceiveDataSize;
                if (ioctlsocket(it->Socket, FIONREAD, &ReceiveDataSize) == SOCKET_ERROR)
                    return eERR_SERVER_RECEIVE;
#else
                int ReceiveDataSize;
                if (ioctl(it->Socket, FIONREAD, &ReceiveDataSize) < 0)
                    return eERR_SERVER_RECEIVE;
#endif
                //����Size��0����,���Client���_�s�u;//
                if (ReceiveDataSize == 0)
                {
                    RemoveClient(it->Socket);

                    it = m_Clients.erase(it);
                }
                else
                {
                    char *Buffer = new char[ReceiveDataSize];
                    memset(Buffer, 0, ReceiveDataSize);
                    recv(it->Socket, Buffer, ReceiveDataSize, 0);

                    //�d�ݬO�_���洤�T��;//
                    if (CheckHandShake(Buffer, ReceiveDataSize) == true)
                    {
                        int ShakeLen = strlen(KSOCKET_HANDSHAKE_CODE);
                        if ((ReceiveDataSize - ShakeLen) > 0)
                        {
                            if (m_RecvCallbackFunc != NULL)
                                m_RecvCallbackFunc(m_CallbackSocket, it->Socket, Buffer + ShakeLen, ReceiveDataSize - ShakeLen);
                        }
                    }
                    else
                    {
                        if (m_RecvCallbackFunc != NULL)
                            m_RecvCallbackFunc(m_CallbackSocket, it->Socket, Buffer, ReceiveDataSize);
                    }
                    delete[] Buffer;
                    Buffer = NULL;

                    //��s�������ƪ��ɶ�;//
#ifdef WIN32
                    it->TimeOut = GetTickCount();
#else
                    gettimeofday(&it->TimeOut, NULL);
#endif
                    it++;
                }
            }
            else
            {
#ifdef WIN32
                unsigned long NowTime = GetTickCount();
                if (NowTime - it->TimeOut > 200000)
#else
                timeval NowTime;
                gettimeofday(&NowTime, NULL);
                if (NowTime.tv_sec - it->TimeOut.tv_sec >= 20)
#endif                  
                {
                    //�L20��S���ǿ�L���,�⦹Client�����_�u;//
                    RemoveClient(it->Socket);

                    it = m_Clients.erase(it);
                }
                else
                {
                    it++;
                }
            }
        }
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

    for (int i = 0; i < (int)m_Clients.size(); i++)
    {
        FD_SET(m_Clients[i].Socket, &SelectFd);
    }
}

int kServer::GetMaxFdNum()
{
    unsigned int MaxNum = m_SocketServer;
    for (int i = 0; i < (int)m_Clients.size(); i++)
    {
        if (m_Clients[i].Socket > MaxNum)
            MaxNum = m_Clients[i].Socket;
    }    

    return MaxNum;
}

bool kServer::AddNewClient( SOCKET Server )
{
    ClientInfo Client;
    memset( &Client, 0, sizeof(ClientInfo));
    
    socklen_t ClientLen = sizeof(sockaddr_in);
    Client.Socket = accept(Server, (struct sockaddr *)&Client.Addr, &ClientLen);
    if (Client.Socket < 0)
        return false;

    //�����إ߳s�u�ɶ�,�ΥH�p��h�[�S�ǰe��Ʈ�,�����_�u;//
#ifdef WIN32
    Client.TimeOut = GetTickCount();
#else
    gettimeofday( &Client.TimeOut, NULL );
#endif

    m_Clients.push_back(Client);
    
    char IP[17];
#ifdef WIN32
    sprintf_s( IP, "%d.%d.%d.%d",                
        Client.Addr.sin_addr.S_un.S_un_b.s_b1,
        Client.Addr.sin_addr.S_un.S_un_b.s_b2,
        Client.Addr.sin_addr.S_un.S_un_b.s_b3,
        Client.Addr.sin_addr.S_un.S_un_b.s_b4 );
#else
    sprintf( IP, "%s", inet_ntoa(Client.Addr.sin_addr) );
#endif

    //�T��;//
    printf( "\n" );
    printf( "[ Accept ]\n" );
    printf( "  Fd No : %d\n", Client.Socket);
    printf( "  Client Port No : %d \n", Client.Addr.sin_port );
    printf( "  Client Address : %s \n", IP );
    printf( "  Total Clients : %d \n", (int)m_Clients.size() );

    if( m_ConnectCallbackFunc != NULL )
        m_ConnectCallbackFunc( m_ConnectWorkingClass, Client.Socket, true, IP );

    return true;
}

void kServer::RemoveClient( SOCKET Clinet )
{
    printf("\n");
    printf("[! Removing !] \n");
    printf("  Remove client on Fd %d \n", Clinet );
    printf("  Total Clients : %d \n", (int)m_Clients.size() - 1);

    if (m_ConnectCallbackFunc != NULL)
        m_ConnectCallbackFunc(m_ConnectWorkingClass, Clinet, false, NULL);

    Close( Clinet );
    FD_CLR( Clinet, &m_SelectFds );
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

    for (int i = 0; i < (int)m_Clients.size(); i++)
    {
        Close(m_Clients[i].Socket);
    }
    m_Clients.clear();
}

