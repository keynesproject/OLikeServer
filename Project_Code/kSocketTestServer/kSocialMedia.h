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

    //���s�����s�u��l��;//
    virtual bool Initial() = 0;

    //�������s�����s�u;//
    virtual bool Close() = 0;

    //�V���s�����ШD���,�ШD�쪺�ƭȷ|�ഫ���r��榡;//
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