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


//�����^�_�����A;//
enum NetReplyType
{
    eNETREPLY_LOGGING = 1,
    eNETREPLY_REQUEST,
    eNETREPLY_DOWNLOAD,
};

//Logging����Ƶ��c;//
struct ClientLoggingInfo
{
    char Account[32];       //�b��;//
    char Password[32];      //�K�X;//
};

//���ҤνШD�N���ɮת����c;//
struct ClientRequestUpdateInfo
{
    char FwVersion[16];     //Firmware����;//
    char PagerType;         //����Type;//
    char Language;          //�y�t;//
    char Band;              //�W��;//
    char SN[26];            //���l���Ǹ�;//
    char ICN[26];           //�Ͳ��N���Ǹ�;//
    char TypeRequest;       //�ШD�n��s�������A0:CodePlug�B1:����;//
};

//���A�����^��ACK�ɷ|Ĳ�o��CallBackFunction;//
typedef void (*SERVER_REPLY_CALLBACK_FUNC)( char ReplyType, bool Success );
void E5CLIENT_API SetReplyCallbackFunc( SERVER_REPLY_CALLBACK_FUNC Func );

//Unication���A���s�u�������s�u;//
bool E5CLIENT_API UniServerConnect( const char *IpV4, int Port );
void E5CLIENT_API UniServerDisconnect();

//�n�JUnication���A��;//
void E5CLIENT_API ServerLogging( ClientLoggingInfo *Logging );

//�ШD�U���ɮ�;//
void E5CLIENT_API ServerRequestData( ClientRequestUpdateInfo *Request );

//���o�U���ɮת��ʤ���;//
int  E5CLIENT_API GetDownloadPercent();

//���o�N���ɮ�;//
void E5CLIENT_API GetBurnData( char **BurnData, int &BurnSize, char **NextFwVersion, char **GetSpVersion );


//�]�w�N�����\�Υ��ѡA��I�s���禡��AGetBurnData��ܱN�|����;//
void E5CLIENT_API SetBurnReply( bool IsSuccess );


#ifdef __cplusplus
}
#endif

#endif //end of #ifndef __E5_CLIENT_API_H__;//

