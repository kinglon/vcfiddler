#include <iostream>
#include "../efd/efd.h"
#include <thread>
#include <string>
#include <fstream>
#include <stdio.h>

using namespace std;

bool __stdcall MyEnableModifyResponseCallbackFun(char* url, char* method)
{
    if (strstr(url, "/purchase/plan/list") && strcmp(method, "GET") == 0)
    {
        return true;
    }

    return false;
}

void __stdcall MyRecvCallback(EFD_pmMessage* pmmessage, char* url, char* method, char* head, char* cookie, unsigned int raw, unsigned int rawLen)
{
    std::string urlstr = url;
    /*std::cout << url << std::endl;
    std::cout << "data length: " << rawLen << std::endl;*/
    if (urlstr.find("/purchase/plan/list") != -1 && strcmp(method, "GET") == 0)
    {       
        ifstream ifs;
        ifs.open("C:\\Users\\zengxiangbin\\Downloads\\response_data.json", ios::in);
        static std::string response_data;
        response_data = "";
        if (ifs.is_open())
        {
            string buf;
            while (getline(ifs, buf))
            {
                response_data += buf;
            }

            ifs.close();
        }

        RecvResetHtml(pmmessage, (char*)response_data.c_str());
    }
}


int main()
{
    StartSSL(true);
    if (!rootCertIsTrusted())
    {
        if (CreateRootCert() == 0)
        {
            std::cout << "failed to create root ert" << std::endl;
            return 0;
        }

        if (InstCert() == 0)
        {
            std::cout << "failed to install root ert" << std::endl;
            return 0;
        }       
    }

    if (InitFiddler(51234, (int)MyRecvCallback, (int)0, (int)MyEnableModifyResponseCallbackFun) == 0)
    {
        std::cout << "failed to init fiddler" << std::endl;
        return 0;
    }

    std::this_thread::sleep_for(std::chrono::hours(1));
    return 0;
}