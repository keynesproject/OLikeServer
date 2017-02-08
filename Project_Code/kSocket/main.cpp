#include "kSocket.h"
#include <curl/curl.h>
#include <iostream>
#include <vector>
#include <sstream>
#include "rapidjson\document.h"

using namespace std;
using namespace rapidjson;

kSocket *g_kServer;

//curl - i - X GET \
//"https://graph.facebook.com/v2.3/121721727936951?access_token=EAACEdEose0cBAIWJ1GGR5EDkYzWJiNZAgNQMkOa1h5Xte7MnlmZBgYHs9OgAe6zXTNcTQb4UCAmSyyyOqDQ3qsxjJgVBop2En6yM4uLmjZCyugiyG0cOlX5RvRHJBom8RlEQbH0kKiTOT6PWRwkPe6PVBRt7ZBJm1oeYkenOh9p59ATZBmQn57hMgzvagI3MZD"

void TestCase1()
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    curl = curl_easy_init();
    if (curl)
    {
        curl_easy_setopt(curl, CURLOPT_URL, "https://graph.facebook.com/v2.3/121721727936951?access_token=275419469142574%7CsvPvM8W-2HC09K9DArg59h5NPE4");

#ifdef SKIP_PEER_VERIFICATION
        /*
        * If you want to connect to a site who isn't using a certificate that is
        * signed by one of the certs in the CA bundle you have, you can skip the
        * verification of the server's certificate. This makes the connection
        * A LOT LESS SECURE.
        *
        * If you have a CA cert for the server stored someplace else than in the
        * default bundle, then the CURLOPT_CAPATH option might come handy for
        * you.
        */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

#ifdef SKIP_HOSTNAME_VERIFICATION
        /*
        * If the site you're connecting to uses a different host name that what
        * they have mentioned in their server certificate's commonName (or
        * subjectAltName) fields, libcurl will refuse to connect. You can skip
        * this check, but this will make the connection less secure.
        */
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
}

static int wait_on_socket(curl_socket_t sockfd, int for_recv, long timeout_ms)
{
    struct timeval tv;
    fd_set infd, outfd, errfd;
    int res;

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    FD_ZERO(&infd);
    FD_ZERO(&outfd);
    FD_ZERO(&errfd);

    FD_SET(sockfd, &errfd); /* always check for error */

    if (for_recv) 
    {
        FD_SET(sockfd, &infd);
    }
    else 
    {
        FD_SET(sockfd, &outfd);
    }

    /* select() returns the number of signalled sockets or -1 */
    res = select(sockfd + 1, &infd, &outfd, &errfd, &tv);
    return res;
}

int TestCase2()
{
    CURL *curl;
    CURLcode res;
    /* Minimalistic http request */
    const char *request = "GET /?id=121721727936951&access_token=275419469142574%7CsvPvM8W-2HC09K9DArg59h5NPE4 HTTP/1.1\r\nHost: graph.facebook.com\r\n\r\n";
    size_t request_len = strlen(request);
    curl_socket_t sockfd;
    size_t nsent_total = 0;

    /* A general note of caution here: if you're using curl_easy_recv() or
    curl_easy_send() to implement HTTP or _any_ other protocol libcurl
    supports "natively", you're doing it wrong and you should stop.

    This example uses HTTP only to show how to use this API, it does not
    suggest that writing an application doing this is sensible.
    */

    curl = curl_easy_init();
    if (curl) 
    {
        //curl_easy_setopt(curl, CURLOPT_URL, "http://example.com");
        curl_easy_setopt(curl, CURLOPT_URL, "https://graph.facebook.com");
        // Do not do the transfer - only connect to host //
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) 
        {
            printf("Error: %s\n", curl_easy_strerror(res));
            return 1;
        }

        /* Extract the socket from the curl handle - we'll need it for waiting. */
        res = curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd);

        if (res != CURLE_OK) 
        {
            printf("Error: %s\n", curl_easy_strerror(res));
            return 1;
        }

        printf("Sending request.\n");

        do {
            /* Warning: This example program may loop indefinitely.
            * A production-quality program must define a timeout and exit this loop
            * as soon as the timeout has expired. */
            size_t nsent;
            do {
                nsent = 0;
                res = curl_easy_send(curl, request + nsent_total, request_len - nsent_total, &nsent);
                nsent_total += nsent;

                if (res == CURLE_AGAIN && !wait_on_socket(sockfd, 0, 60000L))
                {
                    printf("Error: timeout.\n");
                    return 1;
                }
            } while (res == CURLE_AGAIN);

            if (res != CURLE_OK)
            {
                printf("Error: %s\n", curl_easy_strerror(res));
                return 1;
            }

            printf("Sent %" CURL_FORMAT_CURL_OFF_T " bytes.\n", (curl_off_t)nsent);

        } while (nsent_total < request_len);

        printf("Reading response.\n");

        for (;;) 
        {
            /* Warning: This example program may loop indefinitely (see above). */
            char buf[1024];
            size_t nread;
            do {
                nread = 0;
                res = curl_easy_recv(curl, buf, sizeof(buf), &nread);

                if (res == CURLE_AGAIN && !wait_on_socket(sockfd, 1, 60000L))
                {
                    printf("Error: timeout.\n");
                    return 1;
                }
                else
                {
                    printf(buf);
                }
            } while (res == CURLE_AGAIN);

            if (res != CURLE_OK) 
            {
                printf("Error: %s\n", curl_easy_strerror(res));
                break;
            }

            if (nread == 0) 
            {
                /* end of the response */
                break;
            }

            printf("Received %" CURL_FORMAT_CURL_OFF_T " bytes.\n", (curl_off_t)nread);
        }

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    return 0;
}

int TestCase3()
{
    //建立連線資訊;//
    SocketInfo Info;
    memset(&Info, 0, sizeof(SocketInfo));
    Info.Port = 80;
    Info.Type = eTYPE_SERVER;

    //建立Socket;//
    kSocketFactory Factory;
    g_kServer = Factory.CreateSocket(Info);
    if (g_kServer == NULL)
    {
        Factory.CloseSocket(g_kServer);
        return eERR_CREATE;
    }

    //設定接收CallbackFunc;//
    //g_kServer->SetReceiveCallBack((void*)this, &ReceiveDataCallFunc);
    //((kServer*)g_kServer)->SetClientConnectCallBack((void*)this, &ConnectCallFunc);

    //Server啟動;//
    if (g_kServer->SetActive(true) == false)
        return eERR_ACTIVE_NONE;    

    char Data[1024];
    int Len;
    while (1)
    {
        g_kServer->Receive(Data, Len);
        int a = 5;
    }

    return eERR_NONE;
}

void TestCase4()
{
    char Temp[7];
    Temp[6] = 0;

    vector<string> TestStr;
    TestStr.push_back("");
    TestStr.push_back("1");
    TestStr.push_back("12");
    TestStr.push_back("123");
    TestStr.push_back("1234");
    TestStr.push_back("12345");
    TestStr.push_back("123456");
    TestStr.push_back("1234567");
    TestStr.push_back("12345678");
    TestStr.push_back("123456789");
    TestStr.push_back("1234567890");
    TestStr.push_back("12345678901");

    for (int j = 0; j < (int)TestStr.size(); j++)
    {
        for (int i = 5; i >= 0; i--)
        {
            if ((int)TestStr[j].length() - 1 < 5 - i)
                Temp[i] = '0';
            else
                Temp[i] = TestStr[j][(int)TestStr[j].length() - 1 - (5 - i)];
        }

        printf("%s\n", Temp);
    }
}

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void TestCase5()
{
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if (curl) 
    {
        //curl_easy_setopt(curl, CURLOPT_URL, "https://www.google.com");
        curl_easy_setopt(curl, CURLOPT_URL, "https://graph.facebook.com/121721727936951?fields=likes&access_token=275419469142574%7CsvPvM8W-2HC09K9DArg59h5NPE4");        
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        std::cout << readBuffer << std::endl;
    }
}

void TestCase6()
{
    string JsonFile = "{\"likes\": 12345678,\"id\" : \"121721727936951\" }";
    string Field = "likes";
    string Temp;

    Document JsonDoc;
    JsonDoc.Parse(JsonFile.c_str());
        
    if (JsonDoc.HasMember(Field.c_str()))
    {
        Type FieldType = JsonDoc[Field.c_str()].GetType();
        if (FieldType == kNumberType)
        {
            int JsonNum = JsonDoc[Field.c_str()].GetInt();
            stringstream SS;
            SS << JsonNum;
            Temp = SS.str();
        }
        else if (FieldType == kStringType)
        {
            Temp = JsonDoc[Field.c_str()].GetString();
        }
        //return JsonDoc[Field.c_str()].GetInt();
    }
    int a = 5;
}

int main( int argc, char *argv[] )
{
#ifdef PTW32_STATIC_LIB 
    pthread_win32_process_attach_np();
    pthread_win32_thread_attach_np();
    atexit(detach_ptw32);
#endif 
    
    //TestCase1();

    //TestCase2();

    //TestCase3();

    //TestCase4();

    //TestCase5();

    TestCase6();

    system("PAUSE");

    return 0;
}

