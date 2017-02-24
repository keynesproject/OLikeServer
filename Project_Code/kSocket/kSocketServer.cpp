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
    //m_SocketServer = socket( AF_UNIX, SOCK_STREAM, 0 );   //���a�ݳs�u�Alocal to host (pipes, portals);//
    m_SocketServer = socket( AF_INET, SOCK_STREAM, 0 );   //�����s�uTCP�Ainternetwork: UDP, TCP, etc.;//
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

int kTcpServer::Active()
{
    //�]�wPort�i�H���ƨϥ�,�קK�����`�����ɪ�Port���ΰ��D;//
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
    
    printf( "[ StartUp ] Waiting TCP Client Connect!! \n" );

    return eERR_NONE;
}

int kTcpServer::Send( SOCKET ToSocket, char *Data, int DataLen )
{
    //���d�ߦ��L���w��Client Socket;//
    std::vector< ClientInfo >::iterator it = m_Clients.begin();
    for (;; it++)
    {
        if (it == m_Clients.end())
            return eERR_SERVER_SEND_NOT_FOUND_CLIEN;

        if (it->Socket == ToSocket)
            break;
    }

    //�̷�protocol���e�H�e�����w��Socket;//
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
    //�����s�uUDP
    m_SocketServer = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_SocketServer < 0)
        return eERR_OPEN_SOCKET;

    //�D���릡�q�T�]�w;//
    if (SetNonBlocking(m_SocketServer) == false)
        return eERR_BLOCK_SETTING;

    //�]�wServer��T;//
    memset((char *)&m_ServAddr, 0, sizeof(m_ServAddr));
    m_ServAddr.sin_family = AF_INET;                 //�]�w�s�u�˦�;//
    m_ServAddr.sin_port = htons(Info.Port);      //�]�w�s�uPORT���X;//
    m_ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);     //�]�w���A�������Ӫv���󤶭��W���s���ШD;//

    kSocket::GetLocalIp();

    return eERR_NONE;
}

int kUdpServer::Send( SOCKET ToSocket, char *Data, int DataLen)
{
    //���d�ߦ��L���w��Client Socket;//
    std::vector< ClientInfo >::iterator it = m_Clients.begin();
    for (;; it++)
    {
        if (it == m_Clients.end())
            return -1;

        if (it->Socket == ToSocket)
            break;
    }

    //�̷�protocol���e�H�e�����w��Socket;//
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

    //Select���~,�h�X�{��;//
    if (Ret == -1)
    {
        printf("[ Error ] Select error!\n");
        return eERR_SERVER_SELECT;
    }

    //Select�U��,�A������;//
    if (Ret == 0)
    {
        //printf("[ Select ] Continute! \n");
        CheckDisconnectClient();

        return eERR_NONE;
    }

    //���Server���s�u�θ�ƶǿ骺�ШD�q��;//
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
        //����Size��0����,���Client���_�s�u;//
        if (ReceiveDataSize == 0)
        {
            //int a = 5;
        }
        else
        {
            char *Buffer = new char[ReceiveDataSize];
            memset(Buffer, 0, ReceiveDataSize);

            //�����ƾ�;//
            sockaddr_in NewClient;
            socklen_t  ClientLen = sizeof(NewClient);
            int Ret = recvfrom(m_SocketServer, Buffer, ReceiveDataSize, 0, (struct sockaddr *)&NewClient, &ClientLen);
            if (Ret < 0)
            {
                fprintf(stderr, "[ Recvfrom failed ] error no %d\n ", errno);
                perror("recvform call failed");
                //exit(errno);
            }

            //�ˬdClient�O�_�ݭn�s�W�s�u��T;//
            ClientInfo *ConnectClient = CheckClient(NewClient);

            //�d�ݬO�_���洤�T��;//
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
    //�]�wPort�i�H���ƨϥ�,�קK�����`�����ɪ�Port���ΰ��D;//
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
    //���ˬd����ƪ�Client�O�_�s�b;//
    for (size_t i = 0; i < m_Clients.size(); i++)
    {
        if (memcmp(&Addr, &m_Clients[i].Addr, sizeof(sockaddr_in)) == 0)
            return &m_Clients[i];
    }

    //�إߤ@�Ӱߤ@��Socket�s��;//
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

    //����Client���;//
    memcpy(&NewClient.Addr, &Addr, sizeof(sockaddr_in));
       
    //�����إ߳s�u�ɶ�,�ΥH�p��h�[�S�ǰe��Ʈ�,�����_�u;//
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

    //�T��;//
    printf("\n");
    printf("[ UDP Connect ]\n");
    printf("  Fd No : %d\n", NewClient.Socket);
    printf("  Client Port No : %d \n", NewClient.Addr.sin_port);
    printf("  Client Address : %s \n", IP);
    printf("  Total Clients : %d \n", (int)m_Clients.size());
    
    //�[�J�@�ӷs��Client;//
    m_Clients.push_back(NewClient);

    //�q���W�h��Client�s��;//
    if (m_ConnectCallbackFunc != NULL)
        m_ConnectCallbackFunc(m_ConnectWorkingClass, NewClient.Socket, true, IP);
    
    return &m_Clients[m_Clients.size() - 1];
}

void kUdpServer::CheckDisconnectClient()
{
    //�P�_Client Socket�O�_�L�[�S�ǿ�T��,�n�����s�u;//
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