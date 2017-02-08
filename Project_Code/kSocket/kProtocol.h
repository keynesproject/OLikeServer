#ifndef __K_PROTOCOL_H__
#define __K_PROTOCOL_H__

#ifdef WIN32
#include <WinSock2.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// Protocol
/////////////////////////////////////////////////////////////////////////////
#define PROTOCOL_S_REPLY      (char)0x01  //Server�^�_Client��T   �A��Ƶ��c�ѷ�P_Reply;//
#define PROTOCOL_S_RESPONSE   (char)0x02  //Server�w�ɦ^�ǽШD����ơA��Ƶ��c�ѷ�P_S_Response;//

#define PROTOCOL_C_REPLY      (char)0x10  //Client�^�_Server��T   �A��Ƶ��c�ѷ�P_Reply;//
#define PROTOCOL_C_CONNECT    (char)0x11  //Client�s�u�ШD����T    �A��Ƶ��c�ѷ�P_C_Request;//

//�^���t�@�ݥ��ѩΦ��\������;//
enum ProtocolTypeReply
{
    eREPLY_CHECK_ALIVE = 0,
    eREPLY_CONNECT,
    eREPLY_RESPONSE,
};

#pragma pack( push, 1 ) //�O��1�r�`������覡;//

//Portocot�򥻸�Ƶ��c;//
struct ProtocolHeadStruct
{
    char    SyncCode[3]; //�P�B�X�A( OPE: 0X4F,0X50,0X45 )//
    char    Protocol;    //�ǿ�q�T��w����;//
    unsigned short Length;      //�ǿ鵲�c������;//
    unsigned short CRC;         //CRC�ˬd�X;//
};

/////////////////////////////////////////////////////////////////////////////
// Client �ݪ��ШD
/////////////////////////////////////////////////////////////////////////////
//Client�ШD�����;//
struct P_C_Request : public ProtocolHeadStruct
{    
    char SocialMediaType; //���s��������:(1��0X31)FB.(2)Youtube.(3)Twintter;//
    char Machine[15];     //OLike�����W�ٽs��;//
    char ID[24];          //���w��ID;//
    char Field[16];       //���W��;//
};

/////////////////////////////////////////////////////////////////////////////
// Server �ݪ��ШD
/////////////////////////////////////////////////////////////////////////////
struct P_S_Response : public ProtocolHeadStruct
{
    char Number[6];      //Client�ШD������ഫ���r���^��;//
};

/////////////////////////////////////////////////////////////////////////////
// Server �� Client �ݪ�Reply�ҥ~�^��
/////////////////////////////////////////////////////////////////////////////
struct P_Reply : public ProtocolHeadStruct
{
    char TypeReply;         //��Reply�������A�ѷ�ProtocolTypeReply;//
    bool IsSucess;          //���\�Υ���;//
};

#pragma pack(pop) //��_����覡;//

#endif //end of #ifndef __K_PROTOCOL_H__;//

