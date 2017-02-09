#pragma once

#include <sstream>
#include <map>
#include <curl/curl.h>

using namespace std;

class kSocialMedia;

enum SocialMediaType
{
    SM_FB = 1,
    SM_YOUTUBE,
    SM_TWITTER,
    SM_LINE,
};

/////////////////////////////////////////////////////////////////////////////
// kSocialMedia                                                                     
/////////////////////////////////////////////////////////////////////////////
class kSocialMedia
{
public:
    kSocialMedia() {};
    ~kSocialMedia() {};

    //社群網站連線初始化;//
    virtual bool Initial() = 0;

    //關閉社群網站連線;//
    virtual bool Close() = 0;

    //向社群網站請求資料,請求到的數值會轉換成字串格式;//
    virtual string Request(string ID, string Field) = 0;    
};

/////////////////////////////////////////////////////////////////////////////
// Social Media Facebook                                                                   
/////////////////////////////////////////////////////////////////////////////
class ksmFaceBook : public kSocialMedia
{
public:
    ksmFaceBook();
    ~ksmFaceBook();

    bool Initial();

    bool Close();

    string Request(string ID, string Field);

private:
    string ParseJsonField(string Json, string Field);

private:
    CURL *m_Curl;
    curl_socket_t m_Sockfd;
    string m_FbReadBuffer;
};