#ifndef __INCLUDE_PROXY_SERVER
#define __INCLUDE_PROXY_SERVER

#include "headfile.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ServerInfo
    {
        int socketFD;
        struct sockaddr_in serverAddr;
        int bufferSize;

        bool block;
        bool serverIsOpen;
    } ServerInfo;

    ServerInfo *InitServer();

    char WaitSocketResponse(int sockedFeild, unsigned char type, int s, int us);

    bool SetSocketBlockState(int socketFeild, bool isBlock);

    int CreateServer(ServerInfo *server, int port, bool isBlock);

    void ListenServer(ServerInfo *server, int waitTime);

    char *GetConnectIPV4(struct sockaddr_in *sockaddr);

    void FreeConnectIPV4String(char *ipv4);

    void CloseServer(ServerInfo *server);

    void FreeServer(ServerInfo *server);

    bool HandleServerConnect(int clientFD, struct sockaddr_in clientAddr);

#ifdef __cplusplus
}
#endif

#endif