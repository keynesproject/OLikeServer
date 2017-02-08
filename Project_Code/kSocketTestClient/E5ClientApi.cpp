#include "E5ClientApi.h"
#include "kE5Client.h"

#include "kProtocol.h"

pthread_t g_Thread;
static ClientContext g_E5Client;
bool g_IsLoggiong = false;

void *ClientRun( void *Arg )
{
    pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

    while(1)
    {
        if( g_E5Client.Execute() != eERR_NONE )
        {

        }
        Sleep( 1 );
    } 

    return NULL;
}

bool E5CLIENT_API UniServerConnect( const char *IpV4, int Port )
{
    int Error = g_E5Client.ConnectServer( IpV4, Port );
    if(  Error != eERR_NONE )
        return false;

    Error = pthread_create( &g_Thread, NULL, ClientRun, NULL );
   
    return true;
}

void E5CLIENT_API UniServerDisconnect()
{
    pthread_cancel( g_Thread );

    g_IsLoggiong = false;
    g_E5Client.SetState( ClientContext::eSTATE_NONE );

    void *Status = NULL;
    pthread_join( g_Thread, &Status );    
}

void E5CLIENT_API ServerLogging( ClientLoggingInfo *Logging )
{
    ProtocolLogging ClientLogging;
    //memcpy( (&ClientLogging)+sizeof(ProtocolBaseStruct), Logging, sizeof(ProtocolLogging) );
    strcpy_s( ClientLogging.Account, Logging->Account );
    strcpy_s( ClientLogging.Password, Logging->Password );

    ProtocolRequestUpdate *Request = NULL;

    g_E5Client.SetClientInfo( &ClientLogging, Request );
    g_E5Client.SetState( ClientContext::eSTATE_LOGGING );
}

void E5CLIENT_API ServerRequestData( ClientRequestUpdateInfo *Request )
{
    ProtocolRequestUpdate ClientRequest;
    //memcpy( (&ClientRequest)+sizeof(ProtocolBaseStruct), Request, sizeof(ClientRequestUpdateInfo) );
    sprintf_s( ClientRequest.FwVersion, "%s", Request->FwVersion );
    ClientRequest.PagerType = Request->PagerType;
    ClientRequest.Language  = Request->Language;
    ClientRequest.Band      = Request->Band;
    sprintf_s( ClientRequest.SN, "%s", Request->SN );
    sprintf_s( ClientRequest.ICN, "%s", Request->ICN );
    ClientRequest.TypeRequest = Request->TypeRequest;

    ProtocolLogging *Logging = NULL;

    g_E5Client.SetClientInfo( Logging, &ClientRequest );

    g_E5Client.SetState( ClientContext::eSTATE_REQUEST );
}

void E5CLIENT_API SetReplyCallbackFunc( SERVER_REPLY_CALLBACK_FUNC Func )
{
    g_E5Client.SetServerReplyCallFunc( Func );
}

int E5CLIENT_API GetDownloadPercent()
{
    return g_E5Client.GetDownloadFilePercent();
}

void E5CLIENT_API GetBurnData( char **BurnData, int &BurnSize, char **NextFwVersion, char **GetSpVersion )
{
    g_E5Client.GetBurnData( BurnData, BurnSize, NextFwVersion, GetSpVersion );
}

void E5CLIENT_API SetBurnReply( bool IsSuccess )
{
    g_E5Client.ReplyServer( eREPLY_BURN, IsSuccess );
}

