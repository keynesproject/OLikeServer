#include <vector>

#include "kSocket.h"
#include "kE5Client.h"
#include "EncryptionAgent.h"

/**************************************************************************
* ClientContext                                                                     
**************************************************************************/
ClientContext::ClientContext()
: m_Client( NULL )
, m_CurrentState( NULL )
, m_ReplyCallbackFunc( NULL )
, m_EncryAgent( NULL )
, m_DecryptData( NULL )
{
    memset( &m_LoggingInfo, 0, sizeof(ProtocolLogging) );
    memset( &m_RequestInfo, 0, sizeof(ProtocolRequestUpdate) );

    m_ClientStates.insert( make_pair( eSTATE_LOGGING, new StateLogging(this) ) );
    m_ClientStates.insert( make_pair( eSTATE_REQUEST, new StateRequest(this) ) );
    m_ClientStates.insert( make_pair( eSTATE_DOWNLOAD, new StateDownLoad(this) ) );
    m_ClientStates.insert( make_pair( eSTATE_WAIT_REPLY, new StateWatiServerReply(this) ) );

    m_EncryAgent = new EncryptionAgent;
}

ClientContext::~ClientContext()
{
    map< ClientState, BaseState* >::iterator it = m_ClientStates.begin();
    for( ; it != m_ClientStates.end(); it++ )
    {
        delete it->second;
        it->second = NULL;
    }
    m_ClientStates.clear();

    delete m_EncryAgent;
    m_EncryAgent = NULL;

#ifdef AES_ENCRYPTION
    if( m_DecryptData != NULL )
    {
        delete [] m_DecryptData;
        m_DecryptData = NULL;
    }
#endif
}

int ClientContext::ConnectServer( const char *IpV4, int Port )
{
    this->CloseSocket();

    //�إ߳s�u��T;//
    SocketInfo LocalInfo;
    LocalInfo.Type = eTYPE_CLIENT;
    LocalInfo.Port = Port;
    sprintf_s( LocalInfo.IPV4, "%s", IpV4 );
    //sprintf_s( LocalInfo.IPV4, "%s", "127.0.0.1" );

    //�إ�Socket;//
    kSocketFactory Factory;
    m_Client = Factory.CreateSocket( LocalInfo );
    if( m_Client == NULL )
    {
        Factory.CloseSocket( m_Client );
        return eERR_CREATE;
    }

    //�]�w����CallbackFunc;//
    m_Client->SetReceiveCallBack( (void *)this, &ReceiveDataCallFunc );

    //�Ұʳs�u;//
    if( m_Client->SetActive( true ) == false )
        return eERR_ACTIVE_NONE;

    return eERR_NONE;
}

void ClientContext::SetClientInfo( ProtocolLogging *Logging, ProtocolRequestUpdate *Request )
{
    if( Logging != NULL )
        memcpy( &m_LoggingInfo, Logging, sizeof(ProtocolLogging) );

    if( Request != NULL )
        memcpy( &m_RequestInfo, Request, sizeof(ProtocolRequestUpdate) );
}

ProtocolLogging *ClientContext::GetLoggingInfo()
{
    return &m_LoggingInfo;
}

ProtocolRequestUpdate *ClientContext::GetRequestInfo()
{
    return &m_RequestInfo;
}

void ClientContext::SetServerReplyCallFunc( SERVER_REPLY_CALLBACK_FUNC Func )
{
    m_ReplyCallbackFunc = Func;
}

void ClientContext::CallbackReply( int Type, bool Success )
{
    if( m_ReplyCallbackFunc != NULL )
        m_ReplyCallbackFunc( Type, Success );
}

int ClientContext::GetDownloadFilePercent()
{
    return m_DownLoadFilePercent;
}

void ClientContext::SetDownloadFilePercent( int Percent )
{
    if( Percent > 100 )
        m_DownLoadFilePercent = 100;
    else
        m_DownLoadFilePercent = Percent;
}

bool ClientContext::GetBurnData( char **BurnData, int &BurnSize, char **NextFwVersion, char **GetSpVersion )
{
    map< ClientState, BaseState* >::iterator it = m_ClientStates.find( eSTATE_DOWNLOAD );
    if( it != m_ClientStates.end() )
        return ((StateDownLoad*)(it->second))->GetDownloadData( BurnData, BurnSize, NextFwVersion, GetSpVersion );

    return false;
}

void ClientContext::ReleaseBurnData()
{
    map< ClientState, BaseState* >::iterator it = m_ClientStates.find( eSTATE_DOWNLOAD );
    if( it != m_ClientStates.end() )
        return ((StateDownLoad*)(it->second))->ReleaseBurnData();
}

int ClientContext::Send( char Protocol, char *Data )
{
    ProtocolBaseStruct *SendData = (ProtocolBaseStruct*)Data;

    //�]�w��ƪ���;//
    SendData->Length = m_Client->GetProtocolStructSize( Protocol );
    if( SendData->Length == 0 )
        return eERR_PROTOCOL;

    //�[�W Sync �X;//
    SendData->SyncCode[0] = 0x45;
    SendData->SyncCode[1] = 0x35;
    SendData->SyncCode[2] = 0x52;

    //�]�wProtocol;//
    SendData->Protocol = Protocol;

    //�p��CRC;//
    //Client��Srver�b�p���Ƶ��c��CRC�ɳ��O�HCRC=0�h�p�⪺;//
    //�ҥH�H�U�n����CRC�]��0�A�h�p�⵲�G�~�����T����;//
    SendData->CRC = 0;
    SendData->CRC = CalCrcByte( (unsigned char*)SendData, SendData->Length );

    //AES�[�K�H�W���;//
    int SendDataSize = 0;
    char *SnedData = NULL;
    this->EncryptSendData( &Data, SendData->Length, &SnedData, SendDataSize );

    //�ǰe���;//
    //return m_Client->Send( (char *)SendData, SendData->Length );
    int Error = m_Client->Send( SnedData, SendDataSize);

#ifdef AES_ENCRYPTION
    if( SnedData != NULL )
    {
        delete [] SnedData;
        SnedData = NULL;
    }
#endif

    return Error;
}

void ClientContext::EncryptSendData( char **InData, int InSize, char **OutData, int &OutSize )
{
#ifdef AES_ENCRYPTION

    //AES�[�K�H�W���;//
    vector< unsigned char > OriData;
    for( int i = 0; i < InSize; i++ )
        OriData.push_back( (*InData)[i] );   
    vector< unsigned char > EncryptData;
    m_EncryAgent->Encrypt( EncryptionAgent::eMETHOD_AES128, OriData, EncryptData );
    unsigned char *Key = m_EncryAgent->GetEncryptKey();

    //�H�U�����]�íp��CRC;//

    //�]�w��Ƥj�p�νw�Ĥj�p;//
    int  EncryptionStructLen = sizeof(ProtocolEncryption);
    OutSize = EncryptionStructLen + EncryptData.size();
    (*OutData) = new char[OutSize];

    //�]�w��Ƥ��e�έp��CRC;//
    ProtocolEncryption *Encryption = (ProtocolEncryption*)(*OutData);
    Encryption->OriDataLen = InSize;
    Encryption->EncryptDataLen = EncryptData.size();
    memcpy( Encryption->EncryptKey, Key, 16 );
    for (int i=0; i<Encryption->EncryptDataLen; i++ )
        (*OutData)[i+EncryptionStructLen] = EncryptData[i];
    Encryption->CRC = 0;
    Encryption->CRC = CalCrcByte( (unsigned char*)(*OutData), OutSize );

#else //#ifdef AES_ENCRYPTION

    (*OutData) = (*InData);
    OutSize = InSize;

#endif //#ifdef AES_ENCRYPTION
}

static string StateString[] =
{
    "eSTATE_NONE",
    "eSTATE_LOGGING",
    "eSTATE_REQUEST",
    "eSTATE_DOWNLOAD",
    "eSTATE_WAIT_REPLY",
};

const char *GetStateString( int State )
{
    return StateString[State].c_str();
}

static string ReplyString[] =
{
    "eREPLY_CHECK_ALIVE",
    "eREPLY_LOGGING",
    "eREPLY_REQUEST",
    "eREPLY_DOWNLOAD_INFO",
    "eREPLY_DOWNLOAD_DATA",
    "eREPLY_BURN",
    "eREPLY_CRC",
    "eREPLY_SYNC",
    "eREPLY_SIZE_NOT_ENOUGH",
};

const char *GetReplyType( int Type )
{
    return ReplyString[Type].c_str();
}

void ClientContext::ReplyServer( ProtocolTypeReply Type, bool IsSucess )
{
    ProtocolReply Reply;
    memset( &Reply, 0, sizeof(ProtocolReply) );
    Reply.TypeReply = Type;
    Reply.IsSucess = IsSucess;

    this->Send( PROTOCOL_C_REPLY, (char *)&Reply );

    printf( "[ Reply ] Type:%s IsSucess:%d\n", GetReplyType(Type), IsSucess );
}

void ClientContext::SetState( ClientState State )
{
    map< ClientState, BaseState* >::iterator it = m_ClientStates.find( State );
    if( it != m_ClientStates.end() )
        m_CurrentState = it->second;
    else
        m_CurrentState = NULL;

    printf( "[ Set State ] New State -> %s \n", GetStateString(State) );
}

int ClientContext::Execute()
{
    if( m_CurrentState != NULL )
        return m_CurrentState->Execute();

    return eERR_ACTIVE_NONE;
}

void ClientContext::CloseSocket()
{
    if( m_Client != NULL )
    {
        kSocketFactory Factory;
        m_Client->SetActive( false );
        Factory.CloseSocket( m_Client );
        m_Client = NULL;
    }
}

bool ClientContext::DecryptRecvData( char **InData, int InSize, char **OutData )
{
#ifdef AES_ENCRYPTION
    //�����ˬd;//
    if( InSize < sizeof(ProtocolEncryption) )
        return false;

    ProtocolEncryption *EncryptDataStruct = (ProtocolEncryption*)(*InData);

    //CRC ����;//
    int EncryptCrc = EncryptDataStruct->CRC;
    EncryptDataStruct->CRC = 0;
    EncryptDataStruct->CRC = CalCrcByte( (unsigned char*)(*InData), InSize );
    if( EncryptCrc != EncryptDataStruct->CRC )
        return false;

    //�ѱK���;//
    int EncryptionStructLen = sizeof(ProtocolEncryption);
    vector< unsigned char > TempEncryptData;
    for( int i=0; i<EncryptDataStruct->EncryptDataLen; i++ )
        TempEncryptData.push_back( (*InData)[EncryptionStructLen+i] );
    vector< unsigned char > TempDecrypyData;
    m_EncryAgent->Decrypt( EncryptionAgent::eMETHOD_AES128, (unsigned char*)EncryptDataStruct->EncryptKey, TempEncryptData, TempDecrypyData );

    //���o�ѱK���;//
    if( (*OutData) != NULL )
    {
        delete [] (*OutData);
        (*OutData) = NULL;
    }
    (*OutData) = new char[ EncryptDataStruct->OriDataLen ];
    for( int i=0; i<EncryptDataStruct->OriDataLen; i++ )
        (*OutData)[i] = TempDecrypyData[i];

    TempEncryptData.clear();
    TempDecrypyData.clear();

#else //#ifdef AES_ENCRYPTION

    (*OutData) = (*InData);

#endif //#ifdef AES_ENCRYPTION

    return true;
}

int ClientContext::Receive( char *Data, int Size )
{
    if( Data == NULL )
        return eERR_RECV_EMPTY;
    
    if( this->DecryptRecvData( &Data, Size, &m_DecryptData ) == false )        
        return eERR_DECRYPTION;

    //�ˬd�e�Ӹ�ƪ����T��;//
    if( m_Client->CheckSyncCode( m_DecryptData ) == false )
    {
        printf( "[ Receive Error ] Check Sync Code!\n" );
        this->ReplyServer( eREPLY_SYNC, false );
        return eERR_SYNC_CODE;
    }

    //CRC����;//
    ProtocolBaseStruct *ptrRecvStruct = (ProtocolBaseStruct *)m_DecryptData;
    if( m_Client->CheckCRC( ptrRecvStruct ) == false )
    {
        printf( "[ Receive Error ] Check CRC!\n" ); 
        this->ReplyServer( eREPLY_CRC, false );
        return eERR_CRC;
    }

    //���o�q�T��w;//
    char Protocol = m_DecryptData[3];

    int CurrentSize = ptrRecvStruct->Length;
    printf( "[ Receive ] remain size: %d\n", Size - CurrentSize );
    if( Size - CurrentSize < 0 )
    {
        printf( "[ Receive Error ] Size Not Enough!\n" ); 
        this->ReplyServer( eREPLY_SIZE_NOT_ENOUGH, false );
        return eERR_CRC;
    }

    if( m_CurrentState != NULL )
        return m_CurrentState->Receive( Protocol, m_DecryptData );

    return eERR_EMPTY_STATE;
}

int ClientContext::ReceiveDataCallFunc( void *WorkingClass, SOCKET FromSocket, char *Data, int Size )
{
    return ((ClientContext*)WorkingClass)->Receive( Data, Size );
}

void ClientContext::ReleasePtr( char **Ptr )
{
    if( (*Ptr) != NULL )
    {
        delete [] (*Ptr);
        (*Ptr) = NULL;
    }
}

/**************************************************************************
*  BaseState                                                                    
**************************************************************************/
BaseState::BaseState( ClientContext *Context )
: m_Contex( NULL ) 
{
    m_Contex = Context;
}

BaseState::~BaseState()
{

}

int BaseState::Receive( char Protocol, char *Data )
{
    return eERR_NONE;
}

/**************************************************************************
*  Logging State                                                                     
**************************************************************************/
StateLogging::StateLogging( ClientContext *Context )
: BaseState( Context )
{

}

StateLogging::~StateLogging()
{

}

int StateLogging::Execute()
{
    int Error = m_Contex->Send( PROTOCOL_C_LOGGING, (char*)m_Contex->GetLoggingInfo() );

    m_Contex->SetState( ClientContext::eSTATE_WAIT_REPLY );
    return Error;
}

/**************************************************************************
*  Request State
**************************************************************************/
StateRequest::StateRequest( ClientContext *Context )
: BaseState( Context )
{

}
StateRequest::~StateRequest()
{

}

int StateRequest::Execute()
{
    int Error = m_Contex->Send( PROTOCOL_C_REQUST_UPDATE, (char*)m_Contex->GetRequestInfo() );

    m_Contex->SetState( ClientContext::eSTATE_WAIT_REPLY );

    return Error;
}

/**************************************************************************
* Download State                                                                       
**************************************************************************/
StateDownLoad::StateDownLoad( ClientContext *Context )
: BaseState( Context )
, m_MergeReceiveData( NULL )
, m_CanTakeData( false )
{
    memset( &m_UpdateInfo, 0, sizeof(ProtocolUpdateInfo) );

    m_AlreadyReceivevSize = 0;
    m_MergeReceiveDataSize = 0;
}

StateDownLoad::~StateDownLoad()
{
    if( m_MergeReceiveData != NULL )
    {
        delete [] m_MergeReceiveData;
        m_MergeReceiveData = NULL;
    }
}

int StateDownLoad::Execute()
{
    return eERR_NONE;
}

int StateDownLoad::Receive( char Protocol, char *Data )
{
    if( Protocol == PROTOCOL_S_UPDATE_INFO )
    {
        this->ReceiveUpdateInfo( (ProtocolUpdateInfo*)Data );
        return eERR_NONE;
    }
    else if( Protocol == PROTOCOL_S_SEND_UPDATE_DATA )
    {
        this->ReceiveUpdateData( (ProtocolUpdateData*)Data );
        return eERR_NONE;
    }

    return eERR_PROTOCOL;
}

int StateDownLoad::ReceiveUpdateInfo( ProtocolUpdateInfo *Info )
{
    memcpy( &m_UpdateInfo, Info, sizeof(ProtocolUpdateInfo) );

    m_AlreadyReceivevSize = 0;
    m_MergeReceiveDataSize = m_UpdateInfo.FileSize;

    m_Contex->ReplyServer( eREPLY_DOWNLOAD_INFO, true );

    return eERR_NONE;
}

int StateDownLoad::ReceiveUpdateData( ProtocolUpdateData *Data )
{
    printf("[ DownLoad Pack ]Receive, count : %d,  Size : %d \n", Data->No, Data->SegmentSize );

    m_CanTakeData = false;

    m_DataPack.push_back( *Data );

    //�p����o�ɮצʤ���;//
    m_AlreadyReceivevSize += Data->SegmentSize;
    if( m_MergeReceiveDataSize != 0 )
        m_Contex->SetDownloadFilePercent( (int)( ( (float)m_AlreadyReceivevSize / m_MergeReceiveDataSize ) * 100 ) );
    else
        m_Contex->SetDownloadFilePercent( 0 );

    //�^�_Server��������;//
    m_Contex->ReplyServer( eREPLY_DOWNLOAD_DATA, true );

    //�P�_�O�_���̫�@�]���;//
    if( Data->LastSegment )
    {
        //����ɮױ��������A���ɮצX�֪����O;//
        if( m_MergeReceiveData != NULL )
        {
            delete [] m_MergeReceiveData;
            m_MergeReceiveData = NULL;
        }

        //�إ߸�Ƥj�p;//
        m_MergeReceiveData = new char[m_MergeReceiveDataSize+1];
        m_MergeReceiveData[m_MergeReceiveDataSize] = 0x00;

        //�X�ָ��;//
        char *MergePtr = m_MergeReceiveData;
        list< ProtocolUpdateData >::iterator it = m_DataPack.begin();
        for( ; it != m_DataPack.end(); it++ )
        {
            memcpy( MergePtr, it->Data, it->SegmentSize );
            MergePtr += it->SegmentSize;
        }
        m_DataPack.clear();
        //for ( size_t i=0; i<m_DataPack.size(); i++ )
        //{
        //    memcpy( MergePtr, m_DataPack[i].Data, m_DataPack[i].SegmentSize );
        //    MergePtr += m_DataPack[i].SegmentSize;
        //}
        //m_DataPack.clear();

        printf( "[ DownLoad Success ] Size : %d \n", m_MergeReceiveDataSize );

        m_CanTakeData = true;

        //�q���~���i�H���o�U�����;//
        m_Contex->CallbackReply( eNETREPLY_DOWNLOAD, true );

        //��ƤU�������A���A���檬�A;//
        m_Contex->SetState( ClientContext::eSTATE_NONE );
    }

    return eERR_NONE;
}

bool StateDownLoad::GetDownloadData( char **BurnData, int &BurnSize, char **NextFwVersion, char **GetSpVersion )
{
    if( m_CanTakeData == false )
        return false;

    (*BurnData) = m_MergeReceiveData;
    BurnSize = m_MergeReceiveDataSize;

    (*NextFwVersion) = m_UpdateInfo.FwVersion;
    (*GetSpVersion ) = m_UpdateInfo.SpVersion;

    return true;
}

void StateDownLoad::ReleaseBurnData()
{
    if( m_MergeReceiveData != NULL )
    {
        delete [] m_MergeReceiveData;
        m_MergeReceiveData = NULL;
    }
    m_CanTakeData = false;
}

/**************************************************************************
* Wait Reply State                                                                      
**************************************************************************/
StateWatiServerReply::StateWatiServerReply( ClientContext *Context )
: BaseState( Context )
{
}

StateWatiServerReply::~StateWatiServerReply()
{

}

int StateWatiServerReply::Execute()
{
    return eERR_NONE;
}

int StateWatiServerReply::Receive( char Protocol, char *Data )
{
    if( Protocol != PROTOCOL_S_REPLY )
    {
        //��ܳq�T��w���~�A��^�n�J���A;//
        m_Contex->SetState( ClientContext::eSTATE_NONE );
        return eERR_PROTOCOL;
    }

    ProtocolReply *Reply = (ProtocolReply*)Data;   
    switch( Reply->TypeReply )
    {
    case eREPLY_LOGGING:
        //if( Reply->IsSucess )
        //    m_Contex->SetState( ClientContext::eSTATE_REQUEST);

        //���ݥ~���ШD�ɮסA�ҥH������;//
        break;

    case eREPLY_REQUEST:
        if( Reply->IsSucess )
            m_Contex->SetState( ClientContext::eSTATE_DOWNLOAD);
        break;

    default:
        m_Contex->SetState( ClientContext::eSTATE_NONE );
        return eERR_SERVER_REPLY;
    }

    //�q���~��CallBackFunction�������A���\�P�_;//
    m_Contex->CallbackReply( Reply->TypeReply, Reply->IsSucess );

    if( Reply->IsSucess == false )
    {
        //��ܦVSERVER�ШD�����ئ����~�A�@�˪�^��l���A;//
        m_Contex->SetState( ClientContext::eSTATE_NONE );
        return eERR_SERVER_REPLY;
    }

    return eERR_NONE;
}

