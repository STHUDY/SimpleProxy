#include "proxy_server.h"

ServerInfo *InitServer()
{
    ServerInfo *result = (ServerInfo *)malloc(sizeof(ServerInfo));
    if (result != NULL)
    {
        memset(result, 0, sizeof(ServerInfo));
        result->socketFD = 0;
        result->bufferSize = 1024;
        result->serverIsOpen = false;
    }
    return result;
}

char WaitSocketResponse(int sockedField, unsigned char type, int s, int us)
{
    struct timeval timeout;
    timeout.tv_sec = s;
    timeout.tv_usec = us;

    fd_set waitRead, waitWrite, waitExcept;
    int ready;

    // 清空文件描述符集合
    FD_ZERO(&waitRead);
    FD_ZERO(&waitWrite);
    FD_ZERO(&waitExcept);

    if (type == 1)
    { // 等待读
        FD_SET(sockedField, &waitRead);
        ready = select(sockedField + 1, &waitRead, NULL, NULL, &timeout);
    }
    else if (type == 2)
    { // 等待写
        FD_SET(sockedField, &waitWrite);
        ready = select(sockedField + 1, NULL, &waitWrite, NULL, &timeout);
    }
    else if (type == 3)
    { // 等待异常
        FD_SET(sockedField, &waitExcept);
        ready = select(sockedField + 1, NULL, NULL, &waitExcept, &timeout);
    }
    else
    {
        return -2;
    }

    if (ready < 0)
    {
        // 发生错误
        return -1;
    }
    else if (ready == 0)
    {
        // 超时
        return 0;
    }
    else
    {
        // 成功
        return 1;
    }
}

bool SetSocketBlockState(int socketField, bool isBlock)
{
    int flags = fcntl(socketField, F_GETFL, 0);
    if (flags == -1)
    {
        return false;
    }

    if (isBlock)
    {
        flags &= ~O_NONBLOCK; // 清除非阻塞标志，设置为阻塞模式
    }
    else
    {
        flags |= O_NONBLOCK; // 设置非阻塞标志
    }

    if (fcntl(socketField, F_SETFL, flags) == -1)
    {
        return false;
    }

    return true;
}

int CreateServer(ServerInfo *server, int port, bool isBlock)
{
    // 创建信箱
    server->socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socketFD < 0)
    {
        return -1;
    }

    if (SetSocketBlockState(server->socketFD, isBlock))
    {
        server->block = isBlock;
    }

    server->serverAddr.sin_family = AF_INET;
    server->serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    server->serverAddr.sin_port = htons(port);

    int optval = 1;
    if (setsockopt(server->socketFD, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        close(server->socketFD);
        return -1;
    }

    if (bind(server->socketFD, (struct sockaddr *)&server->serverAddr, sizeof(server->serverAddr)) < 0)
        return -2;

    if (listen(server->socketFD, port) < 0)
        return -3;

    return 0;
}

void ListenServer(ServerInfo *server, int waitTime)
{
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    clock_t start, end;
    int clientSocketFeild;
    unsigned char waitTimeState = (server->block) ? ((waitTime != -1) ? 1 : 0) : ((waitTime == -1) ? 2 : 1);

    server->serverIsOpen = true;

    while (server->serverIsOpen)
    {
        if (waitTimeState == 2)
            start = clock();

        memset(&clientAddr, 0, sizeof(clientAddr));
        clientSocketFeild = accept(server->socketFD, (struct sockaddr *)&clientAddr, &clientAddrLen);

        if (waitTimeState == 2)
            end = clock();

        if (clientSocketFeild > 0)
        {
            if (HandleServerConnect(clientSocketFeild, clientAddr))
                close(clientSocketFeild);
        }

        switch (waitTimeState)
        {
        case 1:
            usleep(waitTime);
            break;
        case 2:
            usleep(((end - start) * 1000000) / CLOCKS_PER_SEC);
            break;
        default:
            break;
        }
    }
}

char *GetConnectIPV4(struct sockaddr_in *sockaddr)
{
    char *clientIP = (char *)malloc(INET_ADDRSTRLEN);
    if (clientIP == NULL)
        return NULL;

    if (inet_ntop(AF_INET, &(sockaddr->sin_addr), clientIP, INET_ADDRSTRLEN) == NULL)
    {
        free(clientIP);
        return NULL;
    }

    return clientIP;
}

void FreeConnectIPV4String(char *ipv4)
{
    if (ipv4 != NULL)
        free(ipv4);
}

void CloseServer(ServerInfo *server)
{
    if (server->socketFD)
        close(server->socketFD);
    server->serverIsOpen = false;
}

void FreeServer(ServerInfo *server)
{
    if (server->serverIsOpen)
    {
        CloseServer(server);
    }

    free(server);
}