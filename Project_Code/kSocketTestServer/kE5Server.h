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
    virtual ~ServerContext();

    char *GetClientIP();

    int  Send( char Protocol, char *Data );
    
    void ReplyClient( ProtocolTypeReply Type, bool IsSucess );
    void ShowReplyMsg( bool isGet, ProtocolTypeReply Type, bool IsSucess );

    void SetState( ServerState State );    
    void UndoState();

    int  Execute();
    int  Receive( char *Data, int Size );

    //�]�wClient�ШD�����;//
    bool SetClientRequest(P_C_Request *Data);

    //�^�_Client�ШD�����;//
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
    char   m_SocialMediaType;  //���s��������:(1��0x31)FB.(0x32)Youtube.(0x33)Twintter.(0X34)Line;//    
    string m_ID;               //���w��ID;//
    string m_Field;            //���W��;//
};

/////////////////////////////////////////////////////////////////////////////
// State Base                                                                     
/////////////////////////////////////////////////////////////////////////////
class BaseState
{
public:
    BaseState( ServerContext *Context );
    virtual ~BaseState();

    virtual int Execute();

    virtual int Receive( char Protocol, char *Data );

protected:
    ServerContext *m_Contex;
};

/////////////////////////////////////////////////////////////////////////////
// Connect State : Client�|��n�ШD����T�H�e�L��                                                              
/////////////////////////////////////////////////////////////////////////////
class StateConnect : public BaseState
{
public:
    StateConnect(ServerContext *Context);
    ~StateConnect();

    int Receive(char Protocol, char *Data);
};

/////////////////////////////////////////////////////////////////////////////
// Response State : �w���^��Client�ݽШD�����                    
/////////////////////////////////////////////////////////////////////////////
class StateResponse : public BaseState
{
public:
    StateResponse(ServerContext *Context);
    ~StateResponse();

    int Execute();

    int Receive(char Protocol, char *Data);
    
private:
    clock_t m_ClkStart; //�}�l���ɶ�;//    
};

/////////////////////////////////////////////////////////////////////////////
// Error State : ���~���A
/////////////////////////////////////////////////////////////////////////////
class StateError : public BaseState
{
public:
    StateError(ServerContext *Context);
    ~StateError();

    int Receive(char Protocol, char *Data);
};

