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
    //_mSocket = socket( AF_UNIX, SOCK_STREAM, 0 );   //本地端連線，local to host (pipes, portals);//
    m_SocketServer = socket( AF_INET, SOCK_STREAM, 0 );     //網路連線，internetwork: UDP, TCP, etc.;//
    if( m_SocketServer < 0 )
        return eERR_OPEN_SOCKET;

    //非阻塞式通訊設定;//
    if( SetNonBlocking( m_SocketServer ) == false )
        return eERR_BLOCK_SETTING;

    //設定Server資訊;//
    memset( (char *)&m_ServAddr, 0, sizeof( m_ServAddr ) );
    m_ServAddr.sin_family      = AF_INET;                 //設定連線樣式;//
    m_ServAddr.sin_port        = htons( Info.Port );      //設定連線PORT號碼;//
    m_ServAddr.sin_addr.s_addr = htonl( INADDR_ANY );     //設定伺服器接收來治任何介面上的連結請求;//

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

    //Listen,第二個參數為允許進入的連接數，大多數系統允許最大數字為20;//
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
    //依照protocol內容寄送給指定的Socket;//
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

    //重新設定要Select的子集;//
    ReSetCheckFd(m_SelectFds);
    fd_set ReadFds = m_SelectFds;

    int Res = select(this->GetMaxFdNum() + 1, &ReadFds, (fd_set *)0, (fd_set *)0, &m_SelectTime);

    //select錯誤,基本上為Timeout錯誤;//
    if (Res == -1)
    {
        //printf("[ Error ] Select error Timeout!\n");
        return eERR_SERVER_SELECT;
    }

    //表示沒有需要作用的 SOCKET;//
    if (Res == 0)
    {
        //printf("[ Select ] Continute! \n");
        //判斷Client Socket是否過久沒傳輸訊息,要移除連線;//
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
                //過20秒沒有傳輸過資料,把此Client視為斷線;//
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


    //表示Server Socket有client請求連線的訊息;//
    if (FD_ISSET(m_SocketServer, &ReadFds))
    {
        //加入新的Client連線資訊;//
        AddNewClient(m_SocketServer);
    }
    //表示Client端的Socket有其他的請求;//
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
                //收到Size為0的話,表示Client中斷連線;//
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

                    //查看是否為交握訊息;//
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

                    //更新接收到資料的時間;//
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
                    //過20秒沒有傳輸過資料,把此Client視為斷線;//
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

    //紀錄建立連線時間,用以計算多久沒傳送資料時,視為斷線;//
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

    //訊息;//
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

