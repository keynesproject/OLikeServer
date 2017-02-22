#ifndef __K_PROTOCOL_H__
#define __K_PROTOCOL_H__

#ifdef WIN32
#include <WinSock2.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// Protocol
/////////////////////////////////////////////////////////////////////////////
#define PROTOCOL_S_REPLY      (char)0x01  //Server回復Client資訊   ，資料結構參照P_Reply;//
#define PROTOCOL_S_RESPONSE   (char)0x02  //Server定時回傳請求的資料，資料結構參照P_S_Response;//

#define PROTOCOL_C_REPLY      (char)0x10  //Client回復Server資訊   ，資料結構參照P_Reply;//
#define PROTOCOL_C_CONNECT    (char)0x11  //Client連線請求的資訊    ，資料結構參照P_C_Request;//

#define PROTOCOL_SYNC1  (char)0X4F
#define PROTOCOL_SYNC2  (char)0X50
#define PROTOCOL_SYNC3  (char)0X45

//回應另一端失敗或成功的類型;//
//enum ProtocolTypeReply
//{
//    eREPLY_CHECK_ALIVE = 0,
//    eREPLY_CONNECT,
//    eREPLY_REQUEST_DATA,
//    eREPLY_RESPONSE,
//};

//ProtocolTypeReply 回應另一端失敗或成功的類型;//
#define   REPLY_CHECK_ALIVE  (char)0x01
#define   REPLY_CONNECT      (char)0x01
#define   REPLY_REQUEST_DATA (char)0x02
#define   REPLY_RESPONSE     (char)0x03

#pragma pack( push, 1 ) //保持1字節的對齊方式;//

//Portocot基本資料結構;//
struct ProtocolHeadStruct
{
    char    SyncCode[3]; //同步碼，( OPE: 0X4F,0X50,0X45 )//
    char    Protocol;    //傳輸通訊協定類型;//
    unsigned short Length;      //傳輸結構的長度;//
    unsigned short CRC;         //CRC檢查碼;//
};

/////////////////////////////////////////////////////////////////////////////
// Client 端的請求
/////////////////////////////////////////////////////////////////////////////
//Client請求的資料;//
struct P_C_Request : public ProtocolHeadStruct
{    
    char SocialMediaType; //社群網站類型:(1或0X31)FB.(2)Youtube.(3)Twintter;//
    char Machine[15];     //OLike機器名稱編號;//
    char ID[24];          //指定的ID;//
    char Field[16];       //欄位名稱;//
};

/////////////////////////////////////////////////////////////////////////////
// Server 端的請求
/////////////////////////////////////////////////////////////////////////////
struct P_S_Response : public ProtocolHeadStruct
{
    char Number[6];      //Client請求的欄位轉換成字串後回覆;//
};

/////////////////////////////////////////////////////////////////////////////
// Server 及 Client 端的Reply例外回覆
/////////////////////////////////////////////////////////////////////////////
struct P_Reply : public ProtocolHeadStruct
{
    char TypeReply;         //此Reply的類型，參照ProtocolTypeReply;//
    bool IsSucess;          //成功或失敗;//
};

#pragma pack(pop) //恢復對齊方式;//

#endif //end of #ifndef __K_PROTOCOL_H__;//

