#ifndef __INCLUDE_PROXY_CLIENT
#define __INCLUDE_PROXY_CLIENT

#include "headfile.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ClientInfo
    {
        int socketFD;
        struct sockaddr_in serverAddr;
        struct timeval timeout;
        int bufferSize;
        bool block;

        bool isExist;
    } ClientInfo;

    ClientInfo *InitClient();
    ClientInfo *CreateClient(int socketFD, struct sockaddr_in serverAddr);

    void SetClientConnectTimeout(ClientInfo *client, int s, int us);

    int ConnectServer(ClientInfo *client, char *ip, int port, bool isBlock);

    void CloseConnect(ClientInfo *client);

    void FreeClient(ClientInfo *client);

#ifdef __cplusplus
}
#endif

#endif