#pragma once

#include "headfile.h"

class proxy_worker
{
private:
    /* data */
    int socket_connectA;
    int socket_connectB;

    int loadNum = 0;

    bool isClose = false;

    bool AtoB_isClose = false;
    bool BtoA_isClose = false;

    std::mutex changeProxyStateMutex;

    struct timeval timeout;

public:
    char *connectA_ipv4;
    char *connectB_ipv4;

    int buffer_size = 65535;

    proxy_worker();

    void submitSocketField(int socketFeildA, int socketFeildB);

    void controlAtoB();
    void controlBtoA();

    void setTimeout(int s, int us);

    void waitClose();

    bool checkClose();

    void forceClose();

    ~proxy_worker();
};
