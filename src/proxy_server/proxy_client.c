#include "proxy_client.h"

ClientInfo *InitClient()
{
    ClientInfo *result = (ClientInfo *)malloc(sizeof(ClientInfo));
    if (result != NULL)
    {
        memset(result, 0, sizeof(ClientInfo));
        result->socketFD = 0;
        result->bufferSize = 1024;
    }
    return result;
}

ClientInfo *CreateClient(int socketFD, struct sockaddr_in serverAddr)
{
    ClientInfo *result = (ClientInfo *)malloc(sizeof(ClientInfo));
    if (result != NULL)
    {
        memset(result, 0, sizeof(ClientInfo));
        result->socketFD = socketFD;
        result->serverAddr = serverAddr;
        result->bufferSize = 1024;
    }
    return result;
}

void SetClientConnectTimeout(ClientInfo *client, int s, int us)
{
    client->timeout.tv_sec = s;
    client->timeout.tv_usec = us;
}

int ConnectServer(ClientInfo *client, char *ip, int port, bool isBlock)
{
    struct timeval timeout = client->timeout;
    int nRet, ret;

    client->socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socketFD < 0)
        return -3;

    client->isExist = true;

    if (!isBlock)
    {
        int flags = fcntl(client->socketFD, F_GETFL, 0);
        fcntl(client->socketFD, F_SETFL, flags | O_NONBLOCK);
    }
    client->block = isBlock;

    client->serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &client->serverAddr.sin_addr);
    client->serverAddr.sin_port = htons(port);

    ret = connect(client->socketFD, (struct sockaddr *)&client->serverAddr, sizeof(client->serverAddr));

    if (client->block && ret == 0)
        goto Success;

    fd_set wait;
    FD_ZERO(&wait);
    FD_SET(client->socketFD, &wait);

    nRet = select(client->socketFD + 1, NULL, &wait, NULL, &timeout);

    if (nRet == 0)
        return -1;
    else if (nRet < 0)
        return -2;

Success:
    return 0;
}

void CloseConnect(ClientInfo *client)
{
    if (client->socketFD)
        close(client->socketFD);
    client->isExist = false;
}

void FreeClient(ClientInfo *client)
{
    if (client->isExist)
    {
        CloseConnect(client);
    }
    free(client);
}
