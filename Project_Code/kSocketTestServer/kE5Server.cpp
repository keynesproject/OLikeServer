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

    //�إ߳s�u��T;//
    SocketInfo Info;    
    memset( &Info, 0, sizeof(SocketInfo) );
    Info.Port = OLIKE_SERVER_PORT;
    Info.Type = eTYPE_SERVER;

    //�إ�Socket;//
    kSocketFactory Factory;
    m_kServer = Factory.CreateSocket( Info );
    if( m_kServer == NULL )
    {
        Factory.CloseSocket( m_kServer );
        return eERR_CREATE;
    }

    //�]�w����CallbackFunc;//
    m_kServer->SetReceiveCallBack( (void*)this, &ReceiveDataCallFunc );
    ((kServer*)m_kServer)->SetClientConnectCallBack( (void*)this, &ConnectCallFunc );

    //Server�Ұ�;//
    if( m_kServer->SetActive( true ) == false )
        return eERR_ACTIVE_NONE;
    return eERR_NONE;
}

void ServerAgent::Run()
{
    //Mutex�W��;//
    pthread_mutex_lock( &m_Mutex );
    //pthread_cond_wait( &m_Cond, &m_Mutex );
 
    map< SOCKET, ServerContext* >::iterator it = m_Clients.begin();
    for( ; it != m_Clients.end(); it++ )
        it->second->Execute();

    //Mutex����;//
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
        //���j�M��SOCK�O�_�w�s�b�A�Y���s�b�~�s��;//
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

    //Mutex�W��;//
    pthread_mutex_lock( &Server->m_Mutex );

    map< SOCKET, ServerContext* >::iterator it = Server->m_Clients.find( FromSocket );
    if( it != Server->m_Clients.end() )
    {
        //pthread_cond_wait( &Server->m_Cond, &Server->m_Mutex );
        it->second->Receive( Data, Size );
    }

    //Mutex����;//
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

    //�]�w��ƪ���;//
    SendData->Length = m_kServer->GetProtocolStructSize( Protocol );
    if( SendData->Length == 0 )
        return eERR_PROTOCOL;

    //�[�W Sync �X;//
    SendData->SyncCode[0] = PROTOCOL_SYNC1;
    SendData->SyncCode[1] = PROTOCOL_SYNC2;
    SendData->SyncCode[2] = PROTOCOL_SYNC3;

    //�]�wProtocol;//
    SendData->Protocol = Protocol;

    //�p��CRC;//
    //Client��Srver�b�p���Ƶ��c��CRC�ɳ��O�HCRC=0�h�p�⪺;//
    //�ҥH�H�U�n����CRC�]��0�A�h�p�⵲�G�~�����T����;//
    SendData->CRC = 0;
    //SendData->CRC = (unsigned short)CalCrcByte( (unsigned char*)SendData, SendData->Length );
    
    //�ǰe���;//
    char *Buffer = (char*)SendData;
    int NeedSendSize = SendData->Length;
    while( NeedSendSize )
    {
        //�p��0��ܥX��;//
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
        //����U�Ӫ��A;//
        m_PreviousState = m_CurrentState;
        m_CurrentState = it->second;
    }
    else
    {
        //��ܪ��A���~�A�ҥH�����ܪ�l���A;//
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

    //�ˬd�e�Ӹ�ƪ�Sync�X���T��;//
    if( m_kServer->CheckSyncCode(Data) == false )
    {
        printf( "[ Receive ] Check Sync Code Error!\n" );
        char *DbgMsg = new char[Size + 1];
        memcpy(DbgMsg, Data, Size);
        DbgMsg[Size] = 0;
        printf("[ MSG ] %s\n", DbgMsg);
        return eERR_SYNC_CODE;
    }
    
    //CRC����;//
    ProtocolHeadStruct *ptrRecvStruct = (ProtocolHeadStruct *)Data;
    //if( m_kServer->CheckCRC(ptrRecvStruct) == false )
    //{
    //    printf( "[ Receive ] Check CRC Error!\n" );
    //    return eERR_CRC;
    //}

    //���o�q�T��w;//
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
    //���P�_�ШD�����s�����O�_����,�����Ƥ~���s�إ�;//
    if (Data->SocialMediaType != m_SocialMediaType)
    {
        m_SocialMediaType = Data->SocialMediaType;

        if (m_pSM)
        {
            m_pSM->Close();
            delete m_pSM;
            m_pSM = NULL;
        }

        //�إ߹��������s��������;//
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

        //��l�ƺ����s�u;//
        m_pSM->Initial();
    }

    //�����ШD���;//    
    m_MachineName.assign(Data->Machine);
    m_ID.assign(Data->ID);
    m_Field.assign(Data->Field);

    return true;
}

void ServerContext::ResponseClientData()
{
    //�V���s�����ШD���;//
    string Data = m_pSM->Request(m_ID, m_Field);

    P_S_Response Response;
    memset(&Response, 0, sizeof(P_S_Response));
    
    //���r��̧��ݪ�6���,�������׫e����'0';//
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
// Connect State : Client�|��n�ШD����T�H�e�L��                                                              
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
// Response State : �w���^��Client�ݽШD�����                    
/////////////////////////////////////////////////////////////////////////////
StateResponse::StateResponse(ServerContext *Context)
: BaseState(Context)
{
    //�����إߪ��ɶ�;//
    m_ClkStart = clock();
}

StateResponse::~StateResponse()
{

}

int StateResponse::Execute()
{    
    //�w�ɦV���w�����s�ШD��ƨøѪR�X���w�����(�קאּClient���ШD�~�H�e���);//
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
        //���Client�ܧ�ШD����;//
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
        
        //���Client���^�ж��ةνШD;//
        case PROTOCOL_C_REPLY:
        {
            P_Reply Reply;
            memcpy(&Reply, Data, sizeof(P_Reply));

            //��ܰT��;//
            m_Contex->ShowReplyMsg( true, (ProtocolTypeReply)Reply.TypeReply, Reply.IsSucess);

            //��ܦ���ƽШD,�^�й��������s�������;//
            if (Reply.TypeReply == eREPLY_REQUEST_DATA
                && Reply.IsSucess)
            {
                //�ǰe�ШD��Ƶ�Client;//
                m_Contex->ResponseClientData();
            }
            else if(Reply.TypeReply == eREPLY_RESPONSE)
            {
                //��ܦ����ƸѪR���\;//
                //�ثe��������ʧ@;//
            }
        }
            break;

        default:
            break;
    }

    return eERR_NONE;
}

/////////////////////////////////////////////////////////////////////////////
// Error State : ���~���A
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

    //�����������ШD���A;//
    if( SqlResult == true )
        m_Contex->SetState( ServerContext::eSTATE_REQUEST );

    //�^�e���G;//
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

    //�^�_Client�ШD;//
    m_Contex->ReplyClient( eREPLY_REQUEST, SqlResult );

    m_RequestErrTime++;
    printf( "[ Receive Request ] Result : %d, Times : %d\n", SqlResult, m_RequestErrTime );
    if( SqlResult == true )
    {
        ProtocolUpdateInfo Info;
        Info.FileType = Request->PagerType;
        Info.FileType = Request->TypeRequest;
        
        //�ШD����;//
        if( Request->TypeRequest == Codeplug )
        {
            sprintf_s( Info.FileName, "%s/%s", DIR_CODEPLUG, RequestFile );
            Info.FileSize = 65536;
        }
        else
        {
            sprintf_s( Info.FileName, "%s/%s", DIR_FIRMWARE, RequestFile );

            //�ɮפj�p;//
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
        //����ɮש|���ǰe���A�����o��Client�ݪ��U���ɮצ^��;//
        printf( "[ Send ] No:%d, Size:%d \n", PackData->No, PackData->SegmentSize );
        Error = m_Contex->Send( PROTOCOL_S_SEND_UPDATE_DATA, (char*)PackData );

        //�ǰe�@�]�ɮ׫������Wait Reply���A;//
        //m_Contex->SetState( ServerContext::eSTATE_WAIT_REPLY );
    }
    else
    {
        //����ɮ׳��ǰe�����A�B�o��Client�ݪ��^��;//

        //�M���ɮ�;//
        if( m_OpenDataBuffer != NULL )
        {
            delete [] m_OpenDataBuffer;
            m_OpenDataBuffer = NULL; 
            this->ResetParameter();
        }

        printf( "[ Send ] Update data already send !\n" );

        //�q��SQL�ɮפw�U������;//
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
    // ���m�ɮת�Ū�g��m���Ш��ɮת��}�Y�B, �o�Ϳ��~�h�Ǧ^ 0
    // �ɮ� fp �����O���ĥB�i�� fopen() �}�Ҫ��ɮ�
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

                if (cksum != ch) //check sum �}��
                    done = true;
            }
        }
    }
}

ProtocolUpdateData *StateSendUpdateData::GetPackData( bool IsNextData )
{
    if( this->OpenData() == false )
        return NULL;

    //�̤j�����ɮפj�p;//
    int MaxSegmentSize = sizeof( m_UpdateSegment.Data);
    
    //��l�Ƹ��;//
    memset( &m_UpdateSegment, 0, sizeof(ProtocolUpdateData) );

    //�P�_�O�_�n�H�e�s���ɮ�;//
    if( IsNextData == true )
    {        
        m_SendDataSize += m_LastSendDataSize;
        if( m_LastSendDataSize != 0 )
            m_CurrentPackNo++;
    }

    //�]�w�����ɮפj�p;//
    if( m_SendDataSize + MaxSegmentSize >= m_OpenDataSize )
    {
        m_LastSendDataSize = m_OpenDataSize - m_SendDataSize;
        m_UpdateSegment.LastSegment = true;
    }
    else
    {
        m_LastSendDataSize = MaxSegmentSize;
    }

    //�]�w�ɮ׽s���θ�Ƥ��e;//
    m_UpdateSegment.SegmentSize = m_LastSendDataSize;
    m_UpdateSegment.No = m_CurrentPackNo;
    memcpy( m_UpdateSegment.Data, &m_OpenDataBuffer[m_SendDataSize], m_LastSendDataSize );

    return &m_UpdateSegment;

}

bool StateSendUpdateData::OpenData()
{
    if( m_OpenDataBuffer != NULL )
        return true;

    //���s�]�w�Ѽ�;//
    this->ResetParameter();

    ProtocolUpdateInfo *UpdateInfo = m_Contex->GetReadyToSendInfo();


    //Ū���ɮ�;//
    FILE *File;
    errno_t Err = fopen_s( &File, UpdateInfo->FileName, "rb" );
    if( Err != 0 )
        return false;

    if( UpdateInfo->FileType == Codeplug )
    {
        //�Y�ɮ׬�CodePlug�h���g�L�ഫ;//
        stuffFileString( File , &m_OpenDataBuffer, m_OpenDataSize );
    }
    else
    {
        //���o�ɮפj�p�ëإ߽w�İ�;//
        fseek( File, 0, SEEK_END );
        m_OpenDataSize   = ftell( File );
        m_OpenDataBuffer = new char[ m_OpenDataSize ];
        memset( m_OpenDataBuffer, 0, m_OpenDataSize );

        //Ū���ɮ�;//
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
        //������ǰe�U���ɮת��A��A�A��T���H�e�쨺�Ӫ��A�h�P�_Client Reply���\�P�_;//
        m_Contex->SetState( ServerContext::eSTATE_SEND_DOWNLOAD_DATA );  
        this->SendReplyToState( Data, Reply->Length );
        break;

    case eREPLY_SIZE_NOT_ENOUGH:
    case eREPLY_CRC:
    case eREPLY_SYNC:
        printf( "[ Client Reply ] Type: %d : %d\n", Reply->TypeReply, Reply->IsSucess );
        //���ư��W�Ӫ��A;//
        m_Contex->UndoState();
        this->SendReplyToState( Data, Reply->Length );
        break;

    case eREPLY_BURN:
        printf( "[ Client Reply ] eREPLY_BURN : %d\n", Reply->IsSucess );
        //������N�����\�P�_���^�СA�]�w�����G��������A�^�ШD���A;//
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