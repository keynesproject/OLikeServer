#pragma once

#include "kSocketServer.h"
#include <time.h>
#include <map>


using namespace std;

class BaseState;
class ServerContext;
class kSocialMedia;

/////////////////////////////////////////////////////////////////////////////
// ServerAgent                                                                     
/////////////////////////////////////////////////////////////////////////////
class ServerAgent
{
public:
    ServerAgent();
    ~ServerAgent();

    int Initialize();

    void Run();

    kSocket *GetSocketServer();

private:
    void CloseSocket();

    static int ConnectCallFunc( void* WorkingClass, SOCKET FromSocket, bool IsConnect, char *Ip );
    static int ReceiveDataCallFunc( void *WorkingClass, SOCKET FromSocket, char *Data, int Size );

private:
    kSocket *m_kServer;

    map< SOCKET, ServerContext* > m_Clients;

    pthread_mutex_t m_Mutex;
    pthread_cond_t m_Cond; 
};

/////////////////////////////////////////////////////////////////////////////
// Server Context                                                                     
/////////////////////////////////////////////////////////////////////////////
class ServerContext
{
public:
    enum ServerState
    {
        eSTATE_CONNECT = 0,
        eSTATE_RESPONSE,
        eSTATE_ERROR,
    };

public:
    ServerContext( SOCKET ClientSocket, ServerAgent *Server, char *ClientIp );
    ~ServerContext();

    char *GetClientIP();

    int  Send( char Protocol, char *Data );
    
    void ReplyClient( ProtocolTypeReply Type, bool IsSucess );

    void SetState( ServerState State );    
    void UndoState();

    int  Execute();
    int  Receive( char *Data, int Size );

    //設定Client請求的資料;//
    bool SetClientRequest(P_C_Request *Data);

    //回復Client請求的資料;//
    void ResponseClientData();

private:
    SOCKET  m_Client;
    kSocket *m_kServer;
    char m_ClientIP[17];

    map< ServerState, BaseState* > m_ServerStates;
    BaseState *m_CurrentState;
    BaseState *m_PreviousState;

    kSocialMedia *m_pSM;

    string m_MachineName;
    char   m_SocialMediaType;  //社群網站類型:(1或0x31)FB.(0x32)Youtube.(0x33)Twintter.(0X34)Line;//    
    string m_ID;               //指定的ID;//
    string m_Field;            //欄位名稱;//
};

/////////////////////////////////////////////////////////////////////////////
// State Base                                                                     
/////////////////////////////////////////////////////////////////////////////
class BaseState
{
public:
    BaseState( ServerContext *Context );
    ~BaseState();

    virtual int Execute();

    virtual int Receive( char Protocol, char *Data );

protected:
    ServerContext *m_Contex;
};

/////////////////////////////////////////////////////////////////////////////
// Connect State : Client會把要請求的資訊寄送過來                                                              
/////////////////////////////////////////////////////////////////////////////
class StateConnect : public BaseState
{
public:
    StateConnect(ServerContext *Context);
    ~StateConnect();

    int Receive(char Protocol, char *Data) override;
};

/////////////////////////////////////////////////////////////////////////////
// Response State : 定期回覆Client端請求的資料                    
/////////////////////////////////////////////////////////////////////////////
class StateResponse : public BaseState
{
public:
    StateResponse(ServerContext *Context);
    ~StateResponse();

    int Execute() override;

    int Receive(char Protocol, char *Data) override;

private:
    clock_t m_ClkStart; //開始的時間;//    
};

/////////////////////////////////////////////////////////////////////////////
// Error State : 錯誤狀態
/////////////////////////////////////////////////////////////////////////////
class StateError : public BaseState
{
public:
    StateError(ServerContext *Context);
    ~StateError();

    int Receive(char Protocol, char *Data) override;
};

/*
/////////////////////////////////////////////////////////////////////////////
// Logging State                                                                
/////////////////////////////////////////////////////////////////////////////
class StateLogging : public BaseState
{
public:
    StateLogging( ServerContext *Context );
    ~StateLogging();

    int Receive( char Protocol, char *Data );

};

/////////////////////////////////////////////////////////////////////////////
// Request State                                                                     
/////////////////////////////////////////////////////////////////////////////
class StateRequest : public BaseState
{
public:
    StateRequest( ServerContext *Context );
    ~StateRequest();

    int Receive( char Protocol, char *Data );

private:
    int m_RequestErrTime;
};

/////////////////////////////////////////////////////////////////////////////
// Send Update Info State
/////////////////////////////////////////////////////////////////////////////
class StateSendUpdateInfo : public BaseState
{
public:
    StateSendUpdateInfo( ServerContext *Context );
    ~StateSendUpdateInfo();

    int  Execute();
};

class StateSendUpdateDate : public BaseState
{
public:

};

/////////////////////////////////////////////////////////////////////////////
// Send Update Data State                                                                     
/////////////////////////////////////////////////////////////////////////////
class StateSendUpdateData : public BaseState
{
public:
    StateSendUpdateData( ServerContext *Context );
    ~StateSendUpdateData();

    int Execute();

    int Receive( char Protocol, char *Data );

private:
    bool OpenData();

    ProtocolUpdateData *GetPackData( bool IsNextData );

    void ResetParameter();

private:        
    char *m_OpenDataBuffer;     //開啟的檔案資料;// 
    int   m_OpenDataSize;       //開啟的檔案大小;//

    ProtocolUpdateData m_UpdateSegment; //暫存切割的檔案;//
    bool m_IsSendNextPackData;
    int  m_CurrentPackNo;       //分割的檔案編號;//
    int  m_LastSendDataSize;    //最近一次寄送檔案的大小;//
    int  m_SendDataSize;        //已寄送的檔案大小;//
};

/////////////////////////////////////////////////////////////////////////////
// Wait Client Reply State                                                                     
/////////////////////////////////////////////////////////////////////////////
class StateWaitClientReply : public BaseState
{
public:
    StateWaitClientReply( ServerContext *Context );
    ~StateWaitClientReply();

    int Receive( char Protocol, char *Data );

private:
    void SendReplyToState( char *Data, int DataLength );
};
*/