#pragma once

#include "kSocketClient.h"
#include "E5ClientApi.h"

#include <map>
using namespace std;

class EncryptionAgent;

/**************************************************************************
*  ClientContext                                                                      
**************************************************************************/
class BaseState;
class ClientContext
{
public:
    enum ClientState
    {
        eSTATE_NONE = 0,
        eSTATE_LOGGING,
        eSTATE_REQUEST,
        eSTATE_DOWNLOAD,
        eSTATE_WAIT_REPLY,
    };

public:
    ClientContext();
    ~ClientContext();

    //連接伺服器;//
    int  ConnectServer( const char *IpV4, int Port );

    //設定登入帳號及請求資訊;//
    void SetClientInfo( ProtocolLogging *Logging, ProtocolRequestUpdate *Request );

    ProtocolLogging *GetLoggingInfo();
    ProtocolRequestUpdate *GetRequestInfo();

    void SetServerReplyCallFunc( SERVER_REPLY_CALLBACK_FUNC Func );
    void CallbackReply( int Type, bool Success );

    //設定及取得下載檔案大小的百分比;//
    int  GetDownloadFilePercent();
    void SetDownloadFilePercent( int Percent );

    bool GetBurnData( char **BurnData, int &BurnSize, char **NextFwVersion, char **GetSpVersion );
    void ReleaseBurnData();

    //傳送給伺服器資訊;//
    int  Send( char Protocol, char *Data );

    void ReplyServer( ProtocolTypeReply Type, bool IsSucess );

    void SetState( ClientState State );

    int  Execute();

private:

    void CloseSocket();

    int Receive( char *Data, int Size );
    static int ReceiveDataCallFunc( void *WorkingClass, SOCKET FromSocket, char *Data, int Size );
   
    void EncryptSendData( char **InData, int InSize, char **OutData, int &OutSize );
    bool DecryptRecvData( char **InData, int InSize, char **OutData );
    void ReleasePtr( char **Ptr );

private:    
    kSocket *m_Client;
    ProtocolLogging m_LoggingInfo;
    ProtocolRequestUpdate m_RequestInfo;

    map< ClientState, BaseState* > m_ClientStates;
    BaseState *m_CurrentState;

    SERVER_REPLY_CALLBACK_FUNC m_ReplyCallbackFunc;

    int m_DownLoadFilePercent;

    EncryptionAgent *m_EncryAgent;
    char *m_DecryptData;
};

/**************************************************************************
* BaseState                                                                    
**************************************************************************/
class BaseState
{
public:
    BaseState( ClientContext *Context );
    ~BaseState();

    virtual int Execute() = 0;

    virtual int Receive( char Protocol, char *Data );

protected:
    ClientContext *m_Contex;
};

/**************************************************************************
* Logging State                                                                     
**************************************************************************/
class StateLogging : public BaseState
{
public:
    StateLogging( ClientContext *Context );
    ~StateLogging();

    int Execute();

};

/**************************************************************************
* Request State                                                                     
**************************************************************************/
class StateRequest : public BaseState
{
public:
    StateRequest( ClientContext *Context );
    ~StateRequest();

    int Execute();
};

/**************************************************************************
* Download State                                                                     
**************************************************************************/
class StateDownLoad : public BaseState
{
public:
    StateDownLoad( ClientContext *Context );
    ~StateDownLoad();

    int Execute();

    int Receive( char Protocol, char *Data );

    bool GetDownloadData( char **BurnData, int &BurnSize, char **NextFwVersion, char **GetSpVersion );
    void ReleaseBurnData();

private:

    int ReceiveUpdateInfo( ProtocolUpdateInfo *Info );
    int ReceiveUpdateData( ProtocolUpdateData *Data );

private:

    ProtocolUpdateInfo m_UpdateInfo;
    list< ProtocolUpdateData > m_DataPack;
    char *m_MergeReceiveData;
    int   m_MergeReceiveDataSize;
    int   m_AlreadyReceivevSize;
    bool  m_CanTakeData;
};

/**************************************************************************
* Wait Server Reply State                                                                     
**************************************************************************/
class StateWatiServerReply : public BaseState
{
public:
    StateWatiServerReply( ClientContext *Context );
    ~StateWatiServerReply();

    int Execute();

    int Receive( char Protocol, char *Data );

};

