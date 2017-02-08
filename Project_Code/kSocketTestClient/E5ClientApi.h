#ifndef __E5_CLIENT_API_H__
#define __E5_CLIENT_API_H__

#define E5_CLIENT_EXPORT

#ifdef WIN32
    #ifdef E5_CLIENT_EXPORT
        #define E5CLIENT_API __declspec(dllexport)
    #else 
        #define E5CLIENT_API //__declspec(dllimport)
    #endif 
#else
    #define E5CLIENT_API
#endif 

#ifdef __cplusplus
extern "C"{
#endif


//網路回復的狀態;//
enum NetReplyType
{
    eNETREPLY_LOGGING = 1,
    eNETREPLY_REQUEST,
    eNETREPLY_DOWNLOAD,
};

//Logging的資料結構;//
struct ClientLoggingInfo
{
    char Account[32];       //帳號;//
    char Password[32];      //密碼;//
};

//驗證及請求燒錄檔案的結構;//
struct ClientRequestUpdateInfo
{
    char FwVersion[16];     //Firmware版本;//
    char PagerType;         //本機Type;//
    char Language;          //語系;//
    char Band;              //頻變;//
    char SN[26];            //機子的序號;//
    char ICN[26];           //生產燒錄序號;//
    char TypeRequest;       //請求要更新的類型，0:CodePlug、1:分位;//
};

//伺服器有回傳ACK時會觸發此CallBackFunction;//
typedef void (*SERVER_REPLY_CALLBACK_FUNC)( char ReplyType, bool Success );
void E5CLIENT_API SetReplyCallbackFunc( SERVER_REPLY_CALLBACK_FUNC Func );

//Unication伺服器連線及關閉連線;//
bool E5CLIENT_API UniServerConnect( const char *IpV4, int Port );
void E5CLIENT_API UniServerDisconnect();

//登入Unication伺服器;//
void E5CLIENT_API ServerLogging( ClientLoggingInfo *Logging );

//請求下載檔案;//
void E5CLIENT_API ServerRequestData( ClientRequestUpdateInfo *Request );

//取得下載檔案的百分比;//
int  E5CLIENT_API GetDownloadPercent();

//取得燒錄檔案;//
void E5CLIENT_API GetBurnData( char **BurnData, int &BurnSize, char **NextFwVersion, char **GetSpVersion );


//設定燒錄成功或失敗，當呼叫此函式後，GetBurnData函示將會失效;//
void E5CLIENT_API SetBurnReply( bool IsSuccess );


#ifdef __cplusplus
}
#endif

#endif //end of #ifndef __E5_CLIENT_API_H__;//

