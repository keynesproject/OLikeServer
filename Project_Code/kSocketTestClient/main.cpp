
#include <tchar.h>
#include "kE5Client.h"
#include "E5ClientApi.h"

#define E5_ACCOUNT  "Uni"
#define E5_PASSWORD "123456"
#define E5_SN  "uni00001"

#define E5_FWVERSION "SN : 120303"
#define E5_PAGERTYPE 4
#define E5_LANGUAGE  0
#define E5_BAND      2
#define E5_ICN       "E50001"
#define E5_REQUEST   0


#ifdef _DEBUG
#include <stdlib.h> 
#include <crtdbg.h> 
void EnableMemLeakCheck()
{
    _CrtSetDbgFlag( _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF );
}
#endif

#ifdef PTW32_STATIC_LIB 
static void detach_ptw32(void) 
{ 
    pthread_win32_thread_detach_np(); 
    pthread_win32_process_detach_np(); 
} 
#endif 

void TestInfo( const char *Str )
{
    printf("*********** %s ************ \n", Str );
}

void TestCase1()
{
    int Error = 0;

    ClientContext Client;
    Error = Client.ConnectServer( "192.168.1.82", 8088 );
    if(  Error != eERR_NONE )
        return;

    //設定Client端連線資訊;//
    ProtocolLogging Logging;
    sprintf_s( Logging.Account, "%s", E5_ACCOUNT );
    sprintf_s( Logging.Password, "%s", E5_PASSWORD );

    ProtocolRequestUpdate Request;
    strcpy_s( Request.FwVersion, E5_FWVERSION );
    Request.PagerType = 0;(char)E5_PAGERTYPE;
    Request.Language  = 1;(char)E5_LANGUAGE;
    Request.Band      = 1;(char)E5_BAND;
    strcpy_s( Request.ICN, E5_ICN );
    strcpy_s( Request.SN, E5_SN );
    Request.TypeRequest = 0;

    Client.SetClientInfo( &Logging, &Request );

    //設定起始狀態;//
    Client.SetState( ClientContext::eSTATE_LOGGING );
    while(1)
    {
        if( Client.Execute() != eERR_NONE )
        {

        }
        Sleep( 3 );
    } 
}

static bool g_IsLogging = false;
static bool g_IsRequest = false;
static bool g_CanGetBurnData = false;
void ClientReply( char ReplyType, bool Success )
{
    switch( ReplyType )
    {
    case eNETREPLY_LOGGING:
        g_IsLogging = Success;
        break;

    case eNETREPLY_REQUEST:
        g_IsRequest = Success;
        break;

    case eNETREPLY_DOWNLOAD:
        g_CanGetBurnData = Success;
        if( Success )
        {
            if( g_CanGetBurnData == true )
            {
                char *Data = NULL, *Fw = NULL, *Sp = NULL;
                int size;
                GetBurnData( &Data, size, &Fw, &Sp );
            }
        }
        break;
    }
}

void TestCase2()
{
    //if ( UniServerConnect( "59.124.64.98", 3401 ) == false )
    //    return;

    if ( UniServerConnect( "127.0.0.1", 3401 ) == false )
        return;

    SetReplyCallbackFunc( &ClientReply );
    
    ClientLoggingInfo Logging;
    sprintf_s( Logging.Account, "%s", E5_ACCOUNT );
    sprintf_s( Logging.Password, "%s", E5_PASSWORD );
    ServerLogging( &Logging );

    while( g_IsLogging == false )
    {

    };

    ClientRequestUpdateInfo Request;
    strcpy_s( Request.FwVersion, E5_FWVERSION );
    Request.PagerType = E5_PAGERTYPE;
    Request.Language  = E5_LANGUAGE;
    Request.Band      = E5_BAND;
    strcpy_s( Request.ICN, E5_ICN );
    strcpy_s( Request.SN, E5_SN );
    Request.TypeRequest = E5_REQUEST;
    ServerRequestData( &Request );

    while( g_IsRequest == false )
    {

    }

    int count = 0;
    int Percent=0;
    while(1)
    {
        Percent = GetDownloadPercent();
        if( Percent < 100 )
        {
            //printf( "Percent %d\n" , Percent );
        }
        else
        {
            Sleep(5);
            SetBurnReply( true );
            printf( "Burn update over! \n" );
            break;
        }
    }
}

void TestCase3()
{

}

BOOL WINAPI DllMain (HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
    BOOL result = PTW32_TRUE;
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        result = pthread_win32_process_attach_np ();
        break;
    case DLL_THREAD_ATTACH:
        //A thread is being created;//
        result = pthread_win32_thread_attach_np ();
        break;
    case DLL_THREAD_DETACH:
        //A thread is exiting cleanly;//
        result = pthread_win32_thread_detach_np ();
        break;
    case DLL_PROCESS_DETACH:
        (void) pthread_win32_thread_detach_np ();
        result = pthread_win32_process_detach_np ();
        break;
    }
    return (result);
}

int main( int argc, char *argv[] )
{
#ifdef _DEBUG
    EnableMemLeakCheck();
    //_CrtSetBreakAlloc(133);
#endif

#ifdef PTW32_STATIC_LIB 
    pthread_win32_process_attach_np(); 
    pthread_win32_thread_attach_np(); 
    atexit(detach_ptw32); 
#endif 

    //TestCase1();

    //TestCase2();

    TestCase3();

    system( "PAUSE" );
    return 0;
}




