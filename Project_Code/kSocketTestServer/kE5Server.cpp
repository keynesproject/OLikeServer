#include "kSocket.h"
#include "kE5Server.h"
#include "kSocialMedia.h"

//#define OLIKE_SERVER_PORT 8888 //tcp 8080-8088
#define OLIKE_SERVER_PORT 8090 //udp 8090-8099

/////////////////////////////////////////////////////////////////////////////
// ServerAgent                                                                     
/////////////////////////////////////////////////////////////////////////////
ServerAgent::ServerAgent()
: m_kServer( NULL )
{
    pthread_mutex_init( &m_Mutex, NULL ); 
    pthread_cond_init( &m_Cond, NULL ); 
}

ServerAgent::~ServerAgent()
{
    pthread_mutex_destroy( &m_Mutex );
    pthread_cond_destroy( &m_Cond );
    this->CloseSocket();
}

int ServerAgent::Initialize(SocketType Type)
{
    this->CloseSocket();

    //建立連線資訊;//
    SocketInfo Info;    
    memset( &Info, 0, sizeof(SocketInfo) );
    Info.Port = OLIKE_SERVER_PORT;
    Info.Type = Type;

    //建立Socket;//
    kSocketFactory Factory;
    m_kServer = Factory.CreateSocket( Info );
    if( m_kServer == NULL )
    {
        Factory.CloseSocket( m_kServer );
        return eERR_CREATE;
    }

    //設定接收CallbackFunc;//
    m_kServer->SetReceiveCallBack( (void*)this, &ReceiveDataCallFunc );
    if (Info.Type == eTYPE_TCP_SERVER)
    {
        ((kTcpServer*)m_kServer)->SetClientConnectCallBack((void*)this, &ConnectCallFunc);
    }
    else if(Info.Type == eTYPE_UDP_SERVER)
    {
        ((kUdpServer*)m_kServer)->SetClientConnectCallBack((void*)this, &ConnectCallFunc);
    }

    //Server啟動;//
    if( m_kServer->SetActive( true ) == false )
        return eERR_ACTIVE_NONE;
    return eERR_NONE;
}

void ServerAgent::Run()
{
    //Mutex上鎖;//
    pthread_mutex_lock( &m_Mutex );
    //pthread_cond_wait( &m_Cond, &m_Mutex );
 
    map< SOCKET, ServerContext* >::iterator it = m_Clients.begin();
    for( ; it != m_Clients.end(); it++ )
        it->second->Execute();

    //Mutex解鎖;//
    pthread_mutex_unlock( &m_Mutex );
}

kSocket *ServerAgent::GetSocketServer()
{
    return m_kServer;
}

void ServerAgent::CloseSocket()
{
    if( m_kServer != NULL )
    {
        kSocketFactory Factory;
        m_kServer->SetActive( false );
        Factory.CloseSocket( m_kServer );
        m_kServer = NULL;
    }

    map< SOCKET, ServerContext* >::iterator it = m_Clients.begin();
    for( ; it != m_Clients.end(); it++)
    {
        delete it->second;
        it->second = NULL;
    }
    m_Clients.clear();
}

int ServerAgent::ConnectCallFunc( void* WorkingClass, SOCKET FromSocket, bool IsConnect, char *Ip )
{
    ServerAgent *Server = (ServerAgent*)WorkingClass;

    pthread_mutex_lock( &Server->m_Mutex );

    if( IsConnect == true )
    {
        //先搜尋此SOCK是否已存在，若不存在才新建;//
        map< SOCKET, ServerContext* >::iterator it = Server->m_Clients.find( FromSocket );
        if( it == Server->m_Clients.end() )
        {
            ServerContext *Context = new ServerContext( FromSocket, (ServerAgent*)WorkingClass, Ip );
            Context->SetState( ServerContext::eSTATE_CONNECT);
            Server->m_Clients.insert( make_pair( FromSocket, Context ) );
        }
    }
    else
    {
        map< SOCKET, ServerContext* >::iterator it = Server->m_Clients.begin();
        for( ; it != Server->m_Clients.end(); it++ )
        {
            if( it->first == FromSocket )
            {
                delete it->second;
                it->second = NULL;
                Server->m_Clients.erase( it++ );
                break;
            }
        }
    }

    pthread_mutex_unlock( &Server->m_Mutex );

    return eERR_NONE;
}

int ServerAgent::ReceiveDataCallFunc( void *WorkingClass, SOCKET FromSocket, char *Data, int Size )
{
    ServerAgent *Server = (ServerAgent*)WorkingClass;

    //Mutex上鎖;//
    pthread_mutex_lock( &Server->m_Mutex );

    map< SOCKET, ServerContext* >::iterator it = Server->m_Clients.find( FromSocket );
    if( it != Server->m_Clients.end() )
    {
        //pthread_cond_wait( &Server->m_Cond, &Server->m_Mutex );
        it->second->Receive( Data, Size );
    }

    //Mutex解鎖;//
    pthread_mutex_unlock( &Server->m_Mutex );

    return eERR_NONE;
}

/////////////////////////////////////////////////////////////////////////////
// ServerContext                                                                     
/////////////////////////////////////////////////////////////////////////////
ServerContext::ServerContext( SOCKET ClientSocket, ServerAgent *Server, char *ClientIp )
: m_Client( 0 )
, m_kServer( NULL )
, m_CurrentState( NULL )
, m_pSM( NULL )
, m_SocialMediaType(0)
{
    m_kServer = Server->GetSocketServer();
    m_Client = ClientSocket;
#ifdef WIN32
    strcpy_s(m_ClientIP, ClientIp);
#else
    strcpy(m_ClientIP, ClientIp);
#endif    

    m_ServerStates.insert( make_pair(eSTATE_CONNECT, new StateConnect( this ) ) );
    m_ServerStates.insert( make_pair(eSTATE_RESPONSE, new StateResponse( this ) ) );
}

ServerContext::~ServerContext()
{
    m_CurrentState = NULL;
    
    map< ServerState, BaseState* >::iterator it = m_ServerStates.begin();
    for( ; it != m_ServerStates.end(); it++ )
    {
        delete it->second;
        it->second = NULL;
    }
    m_ServerStates.clear(); 
}

char *ServerContext::GetClientIP()
{
    return m_ClientIP;
}

int ServerContext::Send( char Protocol, char *Data )
{
    int Result = eERR_NONE;
    ProtocolHeadStruct *SendData = (ProtocolHeadStruct*)Data;

    //設定資料長度;//
    SendData->Length = m_kServer->GetProtocolStructSize( Protocol );
    if( SendData->Length == 0 )
        return eERR_PROTOCOL;

    //加上 Sync 碼;//
    SendData->SyncCode[0] = PROTOCOL_SYNC1;
    SendData->SyncCode[1] = PROTOCOL_SYNC2;
    SendData->SyncCode[2] = PROTOCOL_SYNC3;

    //設定Protocol;//
    SendData->Protocol = Protocol;

    //計算CRC;//
    //Client及Srver在計算資料結構的CRC時都是以CRC=0去計算的;//
    //所以以下要先把CRC設為0再去計算結果才為正確的值;//
    SendData->CRC = 0;
    //SendData->CRC = (unsigned short)CalCrcByte( (unsigned char*)SendData, SendData->Length );
    
    //傳送資料;//
    char *Buffer = (char*)SendData;
    int NeedSendSize = SendData->Length;
    while( NeedSendSize )
    {
        //小於0表示出錯;//
        int Ret = m_kServer->Send(m_Client, Buffer, NeedSendSize);
        //int Ret = send( m_Client, Buffer, NeedSendSize, 0 );       
        if( Ret < 0 )
        {
            Result = eERR_SERVER_SEND;

            printf("[Server Send Error] Socket ID:%d, Size:%d \nData:%s\n", m_Client, NeedSendSize, Buffer );

            break;
        }

        Buffer += Ret;
        NeedSendSize -= Ret;

#ifdef WIN32
        Sleep(3);
#else
        usleep(3000);
#endif
    }

    return Result;
}

static const string StateString[] =
{
    "eSTATE_CONNECT",
    "eSTATE_RESPONSE"
};

const char *GetStateString( int State )
{
    return StateString[State].c_str();
}

static const string ReplyString[] =
{
    "REPLY_CHECK_ALIVE",
    "REPLY_CONNECT",
    "REPLY_REQUEST_DATA",
    "REPLY_RESPONSE"
};

const char *GetReplyType( char Type )
{
    return ReplyString[Type].c_str();
}

void ServerContext::ReplyClient( char Type, bool IsSucess )
{
    P_Reply Reply;
    memset( &Reply, 0, sizeof(P_Reply) );
    Reply.TypeReply = Type;
    Reply.IsSucess = IsSucess;

    int Ret = this->Send(PROTOCOL_S_REPLY, (char *)&Reply);
    if (Ret != eERR_NONE )
    {
        return;
    }

    this->ShowReplyMsg( false, Type, IsSucess);
}

void ServerContext::ShowReplyMsg( bool isGet, char Type, bool IsSucess)
{
    if(isGet)
        printf("[ Reply From ] Client:%d Type:%s IsSucess:%d\n", m_Client, GetReplyType(Type), IsSucess);
    else
        printf("[ Reply To ] Client:%d Type:%s IsSucess:%d\n", m_Client, GetReplyType(Type), IsSucess);
}

void ServerContext::SetState( ServerState State )
{
    map< ServerState, BaseState* >::iterator it = m_ServerStates.find( State );
    if( it != m_ServerStates.end() )
    {
        //執行下個狀態;//
        m_PreviousState = m_CurrentState;
        m_CurrentState = it->second;
    }
    else
    {
        //表示狀態錯誤，所以切換至初始狀態;//
        m_CurrentState = m_ServerStates.find(eSTATE_ERROR)->second;
        m_PreviousState = m_CurrentState;
    }

    printf( "[ State ] client: %d! state:%s \n", m_Client, GetStateString(State) );
}

void ServerContext::UndoState()
{
    printf( "[ Undo State ] client: %d! \n", m_Client);
    m_CurrentState = m_PreviousState;
}

int ServerContext::Execute()
{
    if( m_CurrentState != NULL )
        return m_CurrentState->Execute();

    return eERR_ACTIVE_NONE;
}

int ServerContext::Receive( char *Data, int Size )
{
    if( Data == NULL )
        return eERR_RECV_EMPTY;

    //檢查送來資料的Sync碼正確性;//
    if( m_kServer->CheckSyncCode(Data) == false )
    {
        printf( "[ Receive ] Check Sync Code Error!\n" );
        printf("[ MSG ] ");
        for (int i = 0; i < Size; i++)
            printf("%x", Data[i]);
        printf("\n");
        return eERR_SYNC_CODE;
    }
    
    //CRC驗證;//
    ProtocolHeadStruct *ptrRecvStruct = (ProtocolHeadStruct *)Data;
    //if( m_kServer->CheckCRC(ptrRecvStruct) == false )
    //{
    //    printf( "[ Receive ] Check CRC Error!\n" );
    //    return eERR_CRC;
    //}

    //取得通訊協定;//
    char Protocol = Data[3];

    int CurrentSize = ptrRecvStruct->Length;
    printf( "[ Receive ] remain size: %d\n", Size - CurrentSize );
    if( Size - CurrentSize < 0 )
        printf( "[ Receive Error ] Size Not Enough!\n" ); 

    if( m_CurrentState != NULL )
    {
        int Error = m_CurrentState->Receive( Protocol, Data);
        if( Error != eERR_NONE )
            printf( "[ Receive Error ] %d \n", Error );
        return Error;
    }

    return eERR_NONE;
}

bool ServerContext::SetClientRequest(P_C_Request *Data)
{
    //先判斷請求的社群網站是否重複,不重複才重新建立;//
    if (Data->SocialMediaType != m_SocialMediaType)
    {
        m_SocialMediaType = Data->SocialMediaType;

        if (m_pSM)
        {
            m_pSM->Close();
            delete m_pSM;
            m_pSM = NULL;
        }

        //建立對應的社群網站物件;//
        switch (m_SocialMediaType)
        {
        case SM_FB:
            m_pSM = new ksmFaceBook();
            break;

        case SM_YOUTUBE:
            break;

        case SM_TWITTER:
            break;

        case SM_LINE:
            break;

        default:
            return false;
            break;
        }

        //初始化網站連線;//
        m_pSM->Initial();
    }

    //紀錄請求資料;//    
    m_MachineName.assign(Data->Machine);
    m_ID.assign(Data->ID);
    m_Field.assign(Data->Field);

    return true;
}

void ServerContext::ResponseClientData()
{
    //向社群網站請求資料;//    
    string Data = m_pSM->Request(m_ID, m_Field);

    P_S_Response Response;
    memset(&Response, 0, sizeof(P_S_Response));
    
    //取字串最尾端的6位數,不足長度前面補'0';//
    for (int i = 5; i >= 0; i--)
    {        
        if ((int)Data.length()-1 < 5-i )
            Response.Number[i] = '0';
        else
            Response.Number[i] = Data[(int)Data.length() - 1 - (5 - i)];
    }

    this->Send(PROTOCOL_S_RESPONSE, (char *)&Response);

    char Temp[7];
    memcpy(Temp, Response.Number, 6);
    Temp[6] = 0;
    printf("[ Response ] To Client:%d Number:%s \n", m_Client, Temp);
}

/////////////////////////////////////////////////////////////////////////////
// State Base                                                                     
/////////////////////////////////////////////////////////////////////////////
BaseState::BaseState( ServerContext *Context )
{
    m_Contex = Context;
}

BaseState::~BaseState()
{

}

int BaseState::Execute()
{
    return eERR_NONE;
}

int BaseState::Receive( char Protocol, char *Data )
{
    return eERR_NONE;
}

/////////////////////////////////////////////////////////////////////////////
// Connect State : Client會把要請求的資訊寄送過來                                                              
/////////////////////////////////////////////////////////////////////////////
StateConnect::StateConnect(ServerContext *Context)
: BaseState(Context)
{

}

StateConnect::~StateConnect()
{

}

int StateConnect::Receive(char Protocol, char *Data)
{
    if (Protocol != PROTOCOL_C_CONNECT)
    {
        //回應Clinet端連線資訊錯誤;//
        m_Contex->ReplyClient(REPLY_CONNECT, false);
        return eERR_PROTOCOL;
    }

    P_C_Request *Request = (P_C_Request*)Data;   

    if (!m_Contex->SetClientRequest(Request))
    {
        m_Contex->ReplyClient(REPLY_CONNECT, false);
        return eERR_CLIENT_CONNECT_TYPE;
    }
    
    m_Contex->SetState(ServerContext::eSTATE_RESPONSE);

    m_Contex->ReplyClient(REPLY_CONNECT, true);

    return eERR_NONE;
}

/////////////////////////////////////////////////////////////////////////////
// Response State : 定期回覆Client端請求的資料                    
/////////////////////////////////////////////////////////////////////////////
StateResponse::StateResponse(ServerContext *Context)
: BaseState(Context)
{
    //紀錄建立的時間;//
    m_ClkStart = clock();
}

StateResponse::~StateResponse()
{

}

int StateResponse::Execute()
{    
    //定時向指定的社群請求資料並解析出指定的欄位(修改為Client有請求才寄送資料);//
    //if (clock() - m_ClkStart > 5000)
    //{
    //    m_Contex->ResponseClientData();
    //    m_ClkStart = clock();
    //}

    return eERR_NONE;
}

int StateResponse::Receive(char Protocol, char *Data)
{    
    switch (Protocol)
    {
        //表示Client變更請求項目;//
        case PROTOCOL_C_CONNECT:
        {        
            P_C_Request Request;
            memcpy(&Request, Data, sizeof(P_C_Request));

            if (!m_Contex->SetClientRequest(&Request))
            {
                m_Contex->ReplyClient(REPLY_CONNECT, false);
                return eERR_CLIENT_CONNECT_TYPE;
            }

            m_Contex->ReplyClient(REPLY_CONNECT, true);
        }
            break;
        
        //表示Client有回覆項目或請求;//
        case PROTOCOL_C_REPLY:
        {
            P_Reply Reply;
            memcpy(&Reply, Data, sizeof(P_Reply));

            //顯示訊息;//
            m_Contex->ShowReplyMsg( true, Reply.TypeReply, Reply.IsSucess);

            //表示有資料請求,回覆對應的社群網站資料;//
            if (Reply.TypeReply == REPLY_REQUEST_DATA
                && Reply.IsSucess)
            {
                //傳送請求資料給Client;//
                m_Contex->ResponseClientData();
            }
            else if(Reply.TypeReply == REPLY_RESPONSE)
            {
                //表示收到資料解析成功;//
                //目前不做任何動作;//
            }
        }
            break;

        default:
            break;
    }

    return eERR_NONE;
}

/////////////////////////////////////////////////////////////////////////////
// Error State : 錯誤狀態
/////////////////////////////////////////////////////////////////////////////
StateError::StateError(ServerContext *Context)
: BaseState(Context)
{

}

StateError::~StateError()
{

}

int StateError::Receive(char Protocol, char *Data)
{
    return eERR_NONE;
}