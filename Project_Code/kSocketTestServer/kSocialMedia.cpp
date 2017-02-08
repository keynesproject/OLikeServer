#include "kSocialMedia.h"
#include "rapidjson/document.h"
#include <sstream>

using namespace rapidjson;

/////////////////////////////////////////////////////////////////////////////
// Social Media Facebook                                                                   
/////////////////////////////////////////////////////////////////////////////
ksmFaceBook::ksmFaceBook()
{

}

ksmFaceBook::~ksmFaceBook()
{
    
}

bool ksmFaceBook::Initial()
{
    /*
    CURLcode Res;

    m_Curl = curl_easy_init();

    if (!m_Curl)
        return false;

    curl_easy_setopt(m_Curl, CURLOPT_URL, "https://graph.facebook.com");
    
    // Do not do the transfer - only connect to host //
    curl_easy_setopt(m_Curl, CURLOPT_CONNECT_ONLY, 1L);
    Res = curl_easy_perform(m_Curl);

    if (Res != CURLE_OK)
    {
        printf("Error: %s\n", curl_easy_strerror(Res));
        return false;
    }

    // Extract the socket from the curl handle - we'll need it for waiting. ;//
    Res = curl_easy_getinfo(m_Curl, CURLINFO_ACTIVESOCKET, &m_Sockfd);

    if (Res != CURLE_OK)
    {
        printf("Error: %s\n", curl_easy_strerror(Res));
        return false;
    }
    */
    return true;    
}

bool ksmFaceBook::Close() 
{
    if(m_Curl)
        curl_easy_cleanup(m_Curl);

    return true;
}

static size_t FbWriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string ksmFaceBook::Request(string ID, string Field)
{   
    CURLcode Res;
    m_Curl = curl_easy_init();
    if (m_Curl)
    {
        //建立請求網址,原始網址為"https://graph.facebook.com/121721727936951?fields=likes&access_token=275419469142574%7CsvPvM8W-2HC09K9DArg59h5NPE4";//
        string URL = "https://graph.facebook.com/";
        URL.append(ID);
        URL.append("?fields=");
        URL.append(Field);
        URL.append("&access_token=275419469142574%7CsvPvM8W-2HC09K9DArg59h5NPE4");

        curl_easy_setopt(m_Curl, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(m_Curl, CURLOPT_WRITEFUNCTION, FbWriteCallback);
        m_FbReadBuffer.clear();
        curl_easy_setopt(m_Curl, CURLOPT_WRITEDATA, &m_FbReadBuffer);

        Res = curl_easy_perform(m_Curl);
        if (Res != CURLE_OK)
        {
            printf("Error: %s\n", curl_easy_strerror(Res));
            return "";
        }

        curl_easy_cleanup(m_Curl);
        m_Curl = NULL;

        return ParseJsonField(m_FbReadBuffer, Field);
    }

    return "";
}

string ksmFaceBook::ParseJsonField(string Json, string Field)
{
    Document JsonDoc;
    JsonDoc.Parse(Json.c_str());

    if (JsonDoc.HasMember(Field.c_str()))
    {
        Type FieldType = JsonDoc[Field.c_str()].GetType();
        if (FieldType == kNumberType)
        {
            int JsonNum = JsonDoc[Field.c_str()].GetInt();            
            stringstream SS;
            SS << JsonNum;
            return SS.str();
        }
        else if (FieldType == kStringType)
        {
            return JsonDoc[Field.c_str()].GetString();
        }
        //return JsonDoc[Field.c_str()].GetInt();
    }
    
    return "";
}
