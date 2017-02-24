#include "kSocketServer.h"

/////////////////////////////////////////////////////////////////////////////
// kTcpServer                                                                     
/////////////////////////////////////////////////////////////////////////////
kTcpServer::kTcpServer()
: m_SocketServer(0)
, m_ConnectWorkingClass( NULL )
, m_ConnectCallbackFunc( NULL )
{
    m_SelectTime.tv_sec = 0;
    m_SelectTime.tv_usec = 0;
}

kTcpServer::~kTcpServer()
{
    this->CloseAll();
}

int kTcpServer::Create( SocketInfo Info )
{
    //m_SocketServer = socket( AF_UNIX, SOCK_STREAM, 0 );   //本地端連線，local to host (pipes, portals);//
    m_SocketServer = socket( AF_INET, SOCK_STREAM, 0 );   //網路連線TCP，internetwork: UDP, TCP, etc.;//
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

int kTcpServer::Active()
{
    //設定Port可以重複使用,避免不正常關閉時的Port佔用問題;//
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
    
    printf( "[ StartUp ] Waiting TCP Client Connect!! \n" );

    return eERR_NONE;
}

int kTcpServer::Send( SOCKET ToSocket, char *Data, int DataLen )
{
    //先查詢有無指定的Client Socket;//
    std::vector< ClientInfo >::iterator it = m_Clients.begin();
    for (;; it++)
    {
        if (it == m_Clients.end())
            return eERR_SERVER_SEND_NOT_FOUND_CLIEN;

        if (it->Socket == ToSocket)
            break;
    }

    //依照protocol內容寄送給指定的Socket;//
    int Ret = send(ToSocket, Data, DataLen, 0);

    return Ret;
}

int kTcpServer::Receive( char *ReData, int &ReDataLen )
{    
    return eERR_NONE;
}

int kTcpServer::Select()
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

void kTcpServer::SetClientConnectCallBack( void *WorkingClass, CONNECT_CALLBACK_FUNC Func )
{
    if( WorkingClass == NULL || Func == NULL )
        return;

    m_ConnectWorkingClass = WorkingClass;
    m_ConnectCallbackFunc = Func;
}

void kTcpServer::ReSetCheckFd( fd_set &SelectFd )
{
    FD_ZERO( &SelectFd );
    FD_SET( m_SocketServer, &SelectFd );

    for (int i = 0; i < (int)m_Clients.size(); i++)
    {
        FD_SET(m_Clients[i].Socket, &SelectFd);
    }
}

int kTcpServer::GetMaxFdNum()
{
    unsigned int MaxNum = m_SocketServer;
    for (int i = 0; i < (int)m_Clients.size(); i++)
    {
        if (m_Clients[i].Socket > MaxNum)
            MaxNum = m_Clients[i].Socket;
    }    

    return MaxNum;
}

bool kTcpServer::AddNewClient( SOCKET Server )
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

void kTcpServer::RemoveClient( SOCKET Clinet )
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

void kTcpServer::Close( SOCKET Fd )
{
#ifdef WIN32
    closesocket( Fd );
#else
    close( Fd );
#endif
}

void kTcpServer::CloseAll()
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

/////////////////////////////////////////////////////////////////////////////
//  kUdpServer                                                                    
/////////////////////////////////////////////////////////////////////////////
kUdpServer::kUdpServer()
: m_SocketServer(0)
{
}

kUdpServer::~kUdpServer()
{
    this->CloseAll();
}

int kUdpServer::Create(SocketInfo Info)
{
    //網路連線UDP
    m_SocketServer = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_SocketServer < 0)
        return eERR_OPEN_SOCKET;

    //非阻塞式通訊設定;//
    if (SetNonBlocking(m_SocketServer) == false)
        return eERR_BLOCK_SETTING;

    //設定Server資訊;//
    memset((char *)&m_ServAddr, 0, sizeof(m_ServAddr));
    m_ServAddr.sin_family = AF_INET;                 //設定連線樣式;//
    m_ServAddr.sin_port = htons(Info.Port);      //設定連線PORT號碼;//
    m_ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);     //設定伺服器接收來治任何介面上的連結請求;//

    kSocket::GetLocalIp();

    return eERR_NONE;
}

int kUdpServer::Send( SOCKET ToSocket, char *Data, int DataLen)
{
    //先查詢有無指定的Client Socket;//
    std::vector< ClientInfo >::iterator it = m_Clients.begin();
    for (;; it++)
    {
        if (it == m_Clients.end())
            return -1;

        if (it->Socket == ToSocket)
            break;
    }

    //依照protocol內容寄送給指定的Socket;//
    int ClientLen = sizeof(it->Addr);
    int Ret = sendto(m_SocketServer, Data, DataLen, 0, (sockaddr*)&it->Addr, ClientLen);
    
    return Ret;
}

int kUdpServer::Receive(char *ReData, int &ReDataLen)
{
    return eERR_NONE;
}

int kUdpServer::Select()
{
    if (m_isCloseThread == true)
    {
        pthread_exit(NULL);
        return eERR_NONE;
    }

    FD_ZERO(&m_ReadFds);
    FD_SET(m_SocketServer, &m_ReadFds);

    int Ret = select(m_SocketServer + 1, &m_ReadFds, NULL, NULL, &m_SelectTime);

    //Select錯誤,退出程序;//
    if (Ret == -1)
    {
        printf("[ Error ] Select error!\n");
        return eERR_SERVER_SELECT;
    }

    //Select愈時,再次輪巡;//
    if (Ret == 0)
    {
        //printf("[ Select ] Continute! \n");
        CheckDisconnectClient();

        return eERR_NONE;
    }

    //表示Server有連線或資料傳輸的請求通知;//
    if (FD_ISSET(m_SocketServer, &m_ReadFds))
    {
#ifdef WIN32
        unsigned long ReceiveDataSize;
        if (ioctlsocket(m_SocketServer, FIONREAD, &ReceiveDataSize) == SOCKET_ERROR)
            return eERR_SERVER_RECEIVE;
#else
        int ReceiveDataSize;
        if (ioctl(m_SocketServer, FIONREAD, &ReceiveDataSize) < 0)
            return eERR_SERVER_RECEIVE;
#endif
        //收到Size為0的話,表示Client中斷連線;//
        if (ReceiveDataSize == 0)
        {
            //int a = 5;
        }
        else
        {
            char *Buffer = new char[ReceiveDataSize];
            memset(Buffer, 0, ReceiveDataSize);

            //接收數據;//
            sockaddr_in NewClient;
            socklen_t  ClientLen = sizeof(NewClient);
            int Ret = recvfrom(m_SocketServer, Buffer, ReceiveDataSize, 0, (struct sockaddr *)&NewClient, &ClientLen);
            if (Ret < 0)
            {
                fprintf(stderr, "[ Recvfrom failed ] error no %d\n ", errno);
                perror("recvform call failed");
                //exit(errno);
            }

            //檢查Client是否需要新增連線資訊;//
            ClientInfo *ConnectClient = CheckClient(NewClient);

            //查看是否為交握訊息;//
            if (CheckHandShake(Buffer, ReceiveDataSize) == true)
            {
                int ShakeLen = strlen(KSOCKET_HANDSHAKE_CODE);
                if ((ReceiveDataSize - ShakeLen) > 0)
                {
                    if (m_RecvCallbackFunc != NULL)
                        m_RecvCallbackFunc(m_CallbackSocket, ConnectClient->Socket, Buffer + ShakeLen, ReceiveDataSize - ShakeLen);
                }
            }
            else
            {
                if (m_RecvCallbackFunc != NULL)
                    m_RecvCallbackFunc(m_CallbackSocket, ConnectClient->Socket, Buffer, ReceiveDataSize);
            }
            delete[] Buffer;
            Buffer = NULL;
        }
    }
    else
    {
        CheckDisconnectClient();
    }

    return eERR_NONE;
}

SOCKET kUdpServer::GetSocketFd()
{
    return m_SocketServer;
}

void kUdpServer::SetClientConnectCallBack(void *WorkingClass, CONNECT_CALLBACK_FUNC Func)
{
    if (WorkingClass == NULL || Func == NULL)
        return;

    m_ConnectWorkingClass = WorkingClass;
    m_ConnectCallbackFunc = Func;
}

int kUdpServer::Active()
{
    //設定Port可以重複使用,避免不正常關閉時的Port佔用問題;//
    int Yes = 1;
    setsockopt(m_SocketServer, SOL_SOCKET, SO_REUSEADDR, (const char*)&Yes, sizeof(int));

    //Bind;//
    if (bind(m_SocketServer, (struct sockaddr *)&m_ServAddr, sizeof(m_ServAddr)) < 0)
    {
        this->CloseAll();
        return eERR_BIND;
    }

    printf("[ StartUp ] Waiting UDP Client Connect!! \n");

    return eERR_NONE;
}

void kUdpServer::CloseAll()
{
    if (m_SocketServer != 0)
    {
#ifdef WIN32
        closesocket(m_SocketServer);
#else
        close(m_SocketServer);
#endif
    }
}

ClientInfo* kUdpServer::CheckClient(sockaddr_in Addr)
{
    //先檢查此資料的Client是否存在;//
    for (size_t i = 0; i < m_Clients.size(); i++)
    {
        if (memcmp(&Addr, &m_Clients[i].Addr, sizeof(sockaddr_in)) == 0)
            return &m_Clients[i];
    }

    //建立一個唯一的Socket編號;//
    ClientInfo NewClient;    
    NewClient.Socket = 1;
    bool bSameNo = false;
    do
    {
        bSameNo = false;
        for (size_t i = 0; i < m_Clients.size(); i++)
        {
            if (m_Clients[i].Socket == NewClient.Socket)
            {
                bSameNo = true;
                break;
            }
        }

        if (bSameNo)
            NewClient.Socket++;
    } while (bSameNo);

    //紀錄Client資料;//
    memcpy(&NewClient.Addr, &Addr, sizeof(sockaddr_in));
       
    //紀錄建立連線時間,用以計算多久沒傳送資料時,視為斷線;//
#ifdef WIN32
    NewClient.TimeOut = GetTickCount();
#else
    gettimeofday(&NewClient.TimeOut, NULL);
#endif
    
    char IP[17];
#ifdef WIN32
    sprintf_s(IP, "%d.%d.%d.%d",
        NewClient.Addr.sin_addr.S_un.S_un_b.s_b1,
        NewClient.Addr.sin_addr.S_un.S_un_b.s_b2,
        NewClient.Addr.sin_addr.S_un.S_un_b.s_b3,
        NewClient.Addr.sin_addr.S_un.S_un_b.s_b4);
#else
    sprintf(IP, "%s", inet_ntoa(Client.Addr.sin_addr));
#endif

    //訊息;//
    printf("\n");
    printf("[ UDP Connect ]\n");
    printf("  Fd No : %d\n", NewClient.Socket);
    printf("  Client Port No : %d \n", NewClient.Addr.sin_port);
    printf("  Client Address : %s \n", IP);
    printf("  Total Clients : %d \n", (int)m_Clients.size());
    
    //加入一個新的Client;//
    m_Clients.push_back(NewClient);

    //通知上層有Client連結;//
    if (m_ConnectCallbackFunc != NULL)
        m_ConnectCallbackFunc(m_ConnectWorkingClass, NewClient.Socket, true, IP);
    
    return &m_Clients[m_Clients.size() - 1];
}

void kUdpServer::CheckDisconnectClient()
{
    //判斷Client Socket是否過久沒傳輸訊息,要移除連線;//
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
}

void kUdpServer::RemoveClient(SOCKET Clinet)
{
    printf("\n");
    printf("[! Removing Udp !] \n");
    printf("  Remove udp client on socket fd %d \n", Clinet);
    printf("  Total Clients : %d \n", (int)m_Clients.size() - 1);

    if (m_ConnectCallbackFunc != NULL)
        m_ConnectCallbackFunc(m_ConnectWorkingClass, Clinet, false, NULL);
}