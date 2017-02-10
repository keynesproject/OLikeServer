#include "kE5Server.h"
#include <stdlib.h> 

#ifdef _DEBUG
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

int main( int argc, char *argv[] )
{
    srand( GetTickCount() );

#ifdef _DEBUG
    EnableMemLeakCheck();
    //_CrtSetBreakAlloc(135);
#endif   

#ifdef PTW32_STATIC_LIB 
    pthread_win32_process_attach_np(); 
    pthread_win32_thread_attach_np(); 
    atexit(detach_ptw32); 
#endif 

    int Error = eERR_NONE;

    ServerAgent *OLikeServer = new ServerAgent();

    Error = OLikeServer->Initialize();
    if( Error != eERR_NONE )
    {
        delete OLikeServer;
        return Error;
    }

    //±Ò°ÊServer;//
    while(1)
    {
        OLikeServer->Run();
    }

#ifdef PTW32_STATIC_LIB 
    pthread_win32_thread_detach_np();

    pthread_win32_process_detach_np();
#endif 

    return 0;
}

