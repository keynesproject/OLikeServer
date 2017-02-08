#include "kSocket.h"
#include "kE5Server.h"
#include "kSocialMedia.h"

//#define E5_DB_SERVER   "tcp://59.124.64.98:3306"
//#define E5_DB_SERVER   "tcp://localhost:3306"
//#define E5_DB_ACCOUNT  "root"
//#define E5_DB_PASSWORD "123456"

//#define DIR_FIRMWARE "./UpdateData/Firmware"
//#define DIR_CODEPLUG "./UpdateData/CodePlug"

#define OLIKE_SERVER_PORT 80

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

int ServerAgent::Initialize()
{
    this->CloseSocket();

    //建立連線資訊;//
    SocketInfo Info;    
    memset( &Info, 0, sizeof(SocketInfo) );
    Info.Port = OLIKE_SERVER_PORT;
    Info.Type = eTYPE_SERVER;

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
    ((kServer*)m_kServer)->SetClientConnectCallBack( (void*)this, &ConnectCallFunc );

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
    strcpy_s( m_ClientIP, ClientIp );

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
        int Ret = send( m_Client, Buffer, NeedSendSize, 0 );       
        if( Ret <= 0 )
        {
            Result = eERR_SERVER_SEND;
            break;
        }

        Buffer += Ret;
        NeedSendSize -= Ret;

        Sleep( 3 );
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
    "eREPLY_CHECK_ALIVE"
    "eREPLY_CONNECT",
    "eREPLY_REQUEST_DATA",
    "eREPLY_RESPONSE",
};

const char *GetReplyType( int Type )
{
    return ReplyString[Type].c_str();
}

void ServerContext::ReplyClient( ProtocolTypeReply Type, bool IsSucess )
{
    P_Reply Reply;
    memset( &Reply, 0, sizeof(P_Reply) );
    Reply.TypeReply = Type;
    Reply.IsSucess = IsSucess;

    this->Send( PROTOCOL_S_REPLY, (char *)&Reply );

    this->ShowReplyMsg( false, Type, IsSucess);
}

void ServerContext::ShowReplyMsg( bool isGet, ProtocolTypeReply Type, bool IsSucess)
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
        char *DbgMsg = new char[Size + 1];
        memcpy(DbgMsg, Data, Size);
        DbgMsg[Size] = 0;
        printf("[ MSG ] %s\n", DbgMsg);
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
        return eERR_PROTOCOL;

    P_C_Request *Request = (P_C_Request*)Data;   

    if (!m_Contex->SetClientRequest(Request))
    {
        m_Contex->ReplyClient(eREPLY_CONNECT, false);
        return eERR_CLIENT_CONNECT_TYPE;
    }
    
    m_Contex->SetState(ServerContext::eSTATE_RESPONSE);

    m_Contex->ReplyClient(eREPLY_CONNECT, true);

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
                m_Contex->ReplyClient(eREPLY_CONNECT, false);
                return eERR_CLIENT_CONNECT_TYPE;
            }

            m_Contex->ReplyClient(eREPLY_CONNECT, true);
        }
            break;
        
        //表示Client有回覆項目或請求;//
        case PROTOCOL_C_REPLY:
        {
            P_Reply Reply;
            memcpy(&Reply, Data, sizeof(P_Reply));

            //顯示訊息;//
            m_Contex->ShowReplyMsg( true, (ProtocolTypeReply)Reply.TypeReply, Reply.IsSucess);

            //表示有資料請求,回覆對應的社群網站資料;//
            if (Reply.TypeReply == eREPLY_REQUEST_DATA
                && Reply.IsSucess)
            {
                //傳送請求資料給Client;//
                m_Contex->ResponseClientData();
            }
            else if(Reply.TypeReply == eREPLY_RESPONSE)
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

/*
/////////////////////////////////////////////////////////////////////////////
// Logging State                                                                     
/////////////////////////////////////////////////////////////////////////////
StateLogging::StateLogging( ServerContext *Context )
: BaseState( Context )
{

}

StateLogging::~StateLogging()
{

}

int StateLogging::Receive( char Protocol, char *Data )
{
    if( Protocol != PROTOCOL_C_LOGGING )
        return eERR_PROTOCOL;

    ProtocolLogging *Logging = (ProtocolLogging*)Data;
    E5_Db *DB = m_Contex->GetDataBase();
    bool SqlResult = DB->UserVerify( Logging->Account, Logging->Password, m_Contex->GetClientIP() );
    printf( "[ Logging Info ] Account:%s ,Password:%s \n", Logging->Account, Logging->Password );

    //切換成接收請求狀態;//
    if( SqlResult == true )
        m_Contex->SetState( ServerContext::eSTATE_REQUEST );

    //回送結果;//
    m_Contex->ReplyClient( eREPLY_LOGGING, SqlResult );

    return eERR_NONE;
}

/////////////////////////////////////////////////////////////////////////////
// Request State                                                              
/////////////////////////////////////////////////////////////////////////////
StateRequest::StateRequest( ServerContext *Context )
:BaseState( Context )
{
    m_RequestErrTime = 0;
}

StateRequest::~StateRequest()
{

}

int StateRequest::Receive( char Protocol, char *Data )
{
    if( Protocol != PROTOCOL_C_REQUST_UPDATE )
        return eERR_PROTOCOL;

    ProtocolRequestUpdate *Request = (ProtocolRequestUpdate*)Data;
    E5_Db *DB = m_Contex->GetDataBase();
    char RequestFile[64];
    char NextFwVersion[32];
    char SpVersion[32];
    bool SqlResult = DB->SNVerify( 
        Request->SN, 
        Request->ICN,
        (FileType)Request->TypeRequest,
        Request->FwVersion,
        (PagerType)Request->PagerType,
        (LanguageType)Request->Language,
        (BandSplit)Request->Band,
        RequestFile,
        NextFwVersion,
        SpVersion );

    //回復Client請求;//
    m_Contex->ReplyClient( eREPLY_REQUEST, SqlResult );

    m_RequestErrTime++;
    printf( "[ Receive Request ] Result : %d, Times : %d\n", SqlResult, m_RequestErrTime );
    if( SqlResult == true )
    {
        ProtocolUpdateInfo Info;
        Info.FileType = Request->PagerType;
        Info.FileType = Request->TypeRequest;
        
        //請求類型;//
        if( Request->TypeRequest == Codeplug )
        {
            sprintf_s( Info.FileName, "%s/%s", DIR_CODEPLUG, RequestFile );
            Info.FileSize = 65536;
        }
        else
        {
            sprintf_s( Info.FileName, "%s/%s", DIR_FIRMWARE, RequestFile );

            //檔案大小;//
            FILE *File;
            int Error = fopen_s( &File, Info.FileName, "rb" );
            if( Error != 0 )
                Info.FileSize = 0;
            else
            {
                fseek( File, 0, SEEK_END );
                Info.FileSize = ftell( File );
                fclose( File );
            }
        }

        sprintf_s( Info.FwVersion, "%s", NextFwVersion );
        sprintf_s( Info.SpVersion, "%s", SpVersion );
        m_Contex->SetReadyToSendInfo( &Info );

        m_RequestErrTime = 0;
        m_Contex->SetState( ServerContext::eSTATE_SEND_DOWNLOAD_INFO );
    }
    else
    {
        m_Contex->SetState( ServerContext::eSTATE_REQUEST );
    }

    return eERR_NONE;
}


/////////////////////////////////////////////////////////////////////////////
// Send Update Info State                                                                     
/////////////////////////////////////////////////////////////////////////////
StateSendUpdateInfo::StateSendUpdateInfo( ServerContext *Context )
: BaseState( Context )
{

}

StateSendUpdateInfo::~StateSendUpdateInfo()
{

}

int StateSendUpdateInfo::Execute()
{
    m_Contex->SetState( ServerContext::eSTATE_WAIT_REPLY );

    m_Contex->Send( PROTOCOL_S_UPDATE_INFO, (char*)m_Contex->GetReadyToSendInfo());

    return eERR_NONE;
}

/////////////////////////////////////////////////////////////////////////////
*  Send Update Data State                                                            
/////////////////////////////////////////////////////////////////////////////
StateSendUpdateData::StateSendUpdateData( ServerContext *Context )
: BaseState( Context )
, m_OpenDataBuffer( NULL )
{
    this->ResetParameter();    
}

StateSendUpdateData::~StateSendUpdateData()
{
    if( m_OpenDataBuffer != NULL )
    {
        delete [] m_OpenDataBuffer;
        m_OpenDataBuffer = NULL; 
        this->ResetParameter();
    }
}

int StateSendUpdateData::Execute()
{
    int Error = eERR_NONE;

    ProtocolUpdateData *PackData = this->GetPackData( m_IsSendNextPackData );
        
    if( PackData->SegmentSize != 0 )
    {
        //表示檔案尚未傳送完，但有得到Client端的下載檔案回應;//
        printf( "[ Send ] No:%d, Size:%d \n", PackData->No, PackData->SegmentSize );
        Error = m_Contex->Send( PROTOCOL_S_SEND_UPDATE_DATA, (char*)PackData );

        //傳送一包檔案後切換到Wait Reply狀態;//
        //m_Contex->SetState( ServerContext::eSTATE_WAIT_REPLY );
    }
    else
    {
        //表示檔案都傳送完畢，且得到Client端的回應;//

        //清空檔案;//
        if( m_OpenDataBuffer != NULL )
        {
            delete [] m_OpenDataBuffer;
            m_OpenDataBuffer = NULL; 
            this->ResetParameter();
        }

        printf( "[ Send ] Update data already send !\n" );

        //通知SQL檔案已下載完成;//
        m_Contex->GetDataBase()->DownloadACK( true );
        
        //m_Contex->SetState( ServerContext::eSTATE_REQUEST );
    }

    m_Contex->SetState( ServerContext::eSTATE_WAIT_REPLY );

    return Error;
}

int StateSendUpdateData::Receive( char Protocol, char *Data )
{
    if( Protocol != PROTOCOL_C_REPLY )
        return false;

    m_IsSendNextPackData = ((ProtocolReply*)Data)->IsSucess;

    return eERR_NONE;
}

int hex2int(char c)
{
    int i;

    if (c>=0x30 && c<=0x39)
        i = c - 0x30;
    else if (c>=0x41 && c<=0x46)
        i = c - 0x41 + 10;
    else if (c>=0x61 && c<=0x65)
        i = c - 0x61 + 10;
    else i = 0;

    return i;
}

long getOffsetAddress(FILE *fp)
{
    long min;
    char tmp[256];
    long startAddress;
    int first = true;

    rewind(fp);
    while (fgets(tmp,255,fp)!=NULL)
    {
        if ((tmp[0]=='S')&&(tmp[1]=='1'))
        {
            startAddress = hex2int(tmp[4]) * 4096 + hex2int(tmp[5]) * 256 +
                           hex2int(tmp[6]) * 16   + hex2int(tmp[7]);
            if (first)
            {
                first = false;
                min = startAddress;
            }
            else
                min = (min > startAddress) ? startAddress : min;
        }
    }

    return min;
}

void filtLine(char source[])  
{                             
    char target[256];           
    int i,j;                    
    int l;                      
    l = strlen(source);         
    if (l > 256)
        l = 256;
    for (i=0,j=0; i < l; i++)   
    {                           
        if (isprint(source[i]))   
            target[j++] = source[i];
    }                           
    target[j] = '\0';           
    strcpy( source, target);
}

void stuffFileString( FILE *fp, char **Buffer, int &BufferSize )
{   
    // note. delete check 2009.06.11
    int i,noBytes;
    int totalBytes = 0;
    int done = false;
    long startAddress,offsetAddress;
    char tmp[256];
    char ch, cksum;
    BufferSize = 65536;
    (*Buffer) = new char[BufferSize];

    offsetAddress = getOffsetAddress(fp);
    totalBytes = 0;

    // rewind(),
    // 重置檔案的讀寫位置指標到檔案的開頭處, 發生錯誤則傳回 0
    // 檔案 fp 必須是有效且可用 fopen() 開啟的檔案
    rewind(fp);
    while((done==false)&&(fgets(tmp,255,fp)!=NULL))
    {
        if((tmp[0]=='S')&&(tmp[1]=='1'))
        {
            cksum = 0x00;

            // (1*16) + (3-3) = 16
            noBytes = hex2int(tmp[2]) * 16 + hex2int(tmp[3]) - 3;
            filtLine(tmp);
            if((noBytes + 5) * 2 != strlen(tmp))
            {
                done = true;
            }

            if(done==false)
            {
                cksum += noBytes + 3;
                startAddress = (hex2int(tmp[4]) * 4096) + (hex2int(tmp[5]) * 256) +
                    (hex2int(tmp[6]) * 16) + (hex2int(tmp[7]) - offsetAddress);

                cksum += hex2int(tmp[4]) * 16 + hex2int(tmp[5]);
                cksum += hex2int(tmp[6]) * 16 + hex2int(tmp[7]);
                totalBytes += noBytes;

                for(i=0; i<noBytes; i++)
                {
                    ch = hex2int(tmp[8+i*2]) * 16 + hex2int(tmp[9+i*2]);
                    (*Buffer)[startAddress++] = ch;
                    cksum += ch;
                }
                cksum = ~cksum;
                cksum &= 0xFF;

                // line checksum
                ch = hex2int(tmp[8+i*2]) * 16 + hex2int(tmp[9+i*2]);

                if (cksum != ch) //check sum 開關
                    done = true;
            }
        }
    }
}

ProtocolUpdateData *StateSendUpdateData::GetPackData( bool IsNextData )
{
    if( this->OpenData() == false )
        return NULL;

    //最大切割檔案大小;//
    int MaxSegmentSize = sizeof( m_UpdateSegment.Data);
    
    //初始化資料;//
    memset( &m_UpdateSegment, 0, sizeof(ProtocolUpdateData) );

    //判斷是否要寄送新的檔案;//
    if( IsNextData == true )
    {        
        m_SendDataSize += m_LastSendDataSize;
        if( m_LastSendDataSize != 0 )
            m_CurrentPackNo++;
    }

    //設定切割檔案大小;//
    if( m_SendDataSize + MaxSegmentSize >= m_OpenDataSize )
    {
        m_LastSendDataSize = m_OpenDataSize - m_SendDataSize;
        m_UpdateSegment.LastSegment = true;
    }
    else
    {
        m_LastSendDataSize = MaxSegmentSize;
    }

    //設定檔案編號及資料內容;//
    m_UpdateSegment.SegmentSize = m_LastSendDataSize;
    m_UpdateSegment.No = m_CurrentPackNo;
    memcpy( m_UpdateSegment.Data, &m_OpenDataBuffer[m_SendDataSize], m_LastSendDataSize );

    return &m_UpdateSegment;

}

bool StateSendUpdateData::OpenData()
{
    if( m_OpenDataBuffer != NULL )
        return true;

    //重新設定參數;//
    this->ResetParameter();

    ProtocolUpdateInfo *UpdateInfo = m_Contex->GetReadyToSendInfo();


    //讀取檔案;//
    FILE *File;
    errno_t Err = fopen_s( &File, UpdateInfo->FileName, "rb" );
    if( Err != 0 )
        return false;

    if( UpdateInfo->FileType == Codeplug )
    {
        //若檔案為CodePlug則先經過轉換;//
        stuffFileString( File , &m_OpenDataBuffer, m_OpenDataSize );
    }
    else
    {
        //取得檔案大小並建立緩衝區;//
        fseek( File, 0, SEEK_END );
        m_OpenDataSize   = ftell( File );
        m_OpenDataBuffer = new char[ m_OpenDataSize ];
        memset( m_OpenDataBuffer, 0, m_OpenDataSize );

        //讀取檔案;//
        fseek( File, 0, SEEK_SET );
        fread( m_OpenDataBuffer, sizeof( char ), m_OpenDataSize, File );        
    }

    fclose( File );

    return true;
}

void StateSendUpdateData::ResetParameter()
{
    m_CurrentPackNo = 1;
    m_SendDataSize  = 0;
    m_OpenDataSize  = 0;
    m_LastSendDataSize = 0;

    m_IsSendNextPackData = false;
}


/////////////////////////////////////////////////////////////////////////////
// Wait Client Reply State                                                                       
/////////////////////////////////////////////////////////////////////////////
StateWaitClientReply::StateWaitClientReply( ServerContext *Context )
: BaseState( Context )
{

}

StateWaitClientReply::~StateWaitClientReply()
{

}

int StateWaitClientReply::Receive( char Protocol, char *Data )
{
    if( Protocol != PROTOCOL_C_REPLY )
        return eERR_PROTOCOL;

    ProtocolReply *Reply = (ProtocolReply*)Data;
    switch( Reply->TypeReply )
    {
    case eREPLY_DOWNLOAD_INFO:
        {
            printf( "[ Client Reply ] eREPLY_DOWNLOAD_INFO : %d\n", Reply->IsSucess );
            if( Reply->IsSucess == false )
                m_Contex->SetState( ServerContext::eSTATE_SEND_DOWNLOAD_INFO );
            else
            {
                m_Contex->SetState( ServerContext::eSTATE_SEND_DOWNLOAD_DATA );
                this->SendReplyToState( Data, Reply->Length );
            }
        }
        break;

    case eREPLY_DOWNLOAD_DATA:
        printf( "[ Client Reply ] eREPLY_DOWNLOAD_DATA : %d\n", Reply->IsSucess );
        //切換到傳送下載檔案狀態後，再把訊息寄送到那個狀態去判斷Client Reply成功與否;//
        m_Contex->SetState( ServerContext::eSTATE_SEND_DOWNLOAD_DATA );  
        this->SendReplyToState( Data, Reply->Length );
        break;

    case eREPLY_SIZE_NOT_ENOUGH:
    case eREPLY_CRC:
    case eREPLY_SYNC:
        printf( "[ Client Reply ] Type: %d : %d\n", Reply->TypeReply, Reply->IsSucess );
        //重複做上個狀態;//
        m_Contex->UndoState();
        this->SendReplyToState( Data, Reply->Length );
        break;

    case eREPLY_BURN:
        printf( "[ Client Reply ] eREPLY_BURN : %d\n", Reply->IsSucess );
        //接收到燒錄成功與否的回覆，設定完結果後切換狀態回請求狀態;//
        m_Contex->GetDataBase()->WritePagerACK( Reply->IsSucess );
        m_Contex->SetState( ServerContext::eSTATE_REQUEST );
        break;

    default:
        return eERR_CLIENT_REPLY;
    }

    return eERR_NONE;
}

void StateWaitClientReply::SendReplyToState( char *Data, int DataLength )
{
#ifdef AES_ENCRYPTION

    int EncryReplySize = 0;
    char *EncryReplyData = NULL;
    m_Contex->EncryptSendData( &Data, DataLength, &EncryReplyData, EncryReplySize );
    m_Contex->Receive( EncryReplyData, EncryReplySize );

#else

    m_Contex->Receive( Data, DataLength );

#endif

    m_Contex->Execute();
}
*/