#include "proxy_worker.hpp"

proxy_worker::proxy_worker() = default;

void proxy_worker::submitSocketField(int socketFeildA, int socketFeildB)
{
    this->socket_connectA = socketFeildA;
    this->socket_connectB = socketFeildB;
}

void proxy_worker::controlAtoB()
{
    std::unique_lock<std::mutex> lock(this->changeProxyStateMutex);
    this->AtoB_isClose = false;
    this->loadNum += 1;
    lock.unlock();
    char *buffer = nullptr;
    try
    {
        buffer = new char[this->buffer_size];
    }
    catch (const std::bad_alloc &e)
    {
        // 处理内存分配失败的情况
        std::cerr << "[ERROR]Memory allocation failed: " << e.what() << std::endl;
        goto _End;
    }
    // std::cout << "已创建" << this->connectA_ipv4 << "->" << this->connectB_ipv4 << "的映射\n";
    while (!this->isClose)
    {
        if (this->BtoA_isClose)
        {
            break;
        }
        struct pollfd pfd;
        pfd.fd = this->socket_connectA;
        pfd.events = POLLIN | POLLHUP | POLLERR;

        int ret = poll(&pfd, 1, 0);
        if (ret > 0)
        {
            if (pfd.revents & POLLHUP)
            {
                break;
            }
        }

        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(this->socket_connectA, SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0)
        {
            break;
        }

        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds);
        FD_ZERO(&exceptfds);
        FD_SET(this->socket_connectA, &readfds);
        FD_SET(this->socket_connectA, &exceptfds);

        struct timeval timeoutUse = this->timeout;

        int result = select(this->socket_connectA + 1, &readfds, NULL, &exceptfds, &timeoutUse);

        if (result < 0)
        {
            break;
        }

        usleep(1000);

        if (result == 0)
        {
            char buffer[1];
            ssize_t bytes_received = recv(this->socket_connectA, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);
            if (bytes_received == 0)
            {
                break;
            }
            else if (bytes_received == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    break;
                }
            }
        }

        if (FD_ISSET(this->socket_connectA, &readfds))
        {
            ssize_t recv_n = recv(this->socket_connectA, buffer, this->buffer_size, 0);
            if (recv_n <= 0)
            {
                break;
            }

            FD_ZERO(&writefds);
            FD_ZERO(&exceptfds);
            FD_SET(this->socket_connectB, &writefds);
            FD_SET(this->socket_connectB, &exceptfds);

            timeoutUse = this->timeout;

            result = select(this->socket_connectB + 1, NULL, &writefds, &exceptfds, &timeoutUse);
            if (result < 0)
            {
                break;
            }

            if (FD_ISSET(this->socket_connectB, &writefds))
            {
                size_t send_n = 0;
                while (send_n < recv_n)
                {
                    ssize_t sent = send(this->socket_connectB, buffer + send_n, recv_n - send_n, MSG_NOSIGNAL);
                    if (sent <= 0)
                    {
                        // 发送失败或连接关闭
                        break;
                    }
                    send_n += sent; // 累加已发送的字节数
                }
                lock.lock();
                Gloabl_global_info.send_size += send_n;
                lock.unlock();
                // std::cout << this->connectA_ipv4 << "发送数据：" << this->connectB_ipv4 << " (" << send_n << "Bytes" << ")\n";

                if (send_n < recv_n)
                {
                    break;
                }
            }

            if (FD_ISSET(this->socket_connectB, &exceptfds))
            {
                break;
            }
        }

        if (FD_ISSET(this->socket_connectA, &exceptfds))
        {
            break;
        }
    }
    delete[] buffer;
_End:
    lock.lock();
    this->AtoB_isClose = true;
    lock.unlock();
    shutdown(this->socket_connectA, SHUT_RD);
    shutdown(this->socket_connectB, SHUT_WR);
    // std::cout << "已关闭" << this->connectA_ipv4 << "->" << this->connectB_ipv4 << "的映射\n";
}

void proxy_worker::controlBtoA()
{
    std::unique_lock<std::mutex> lock(this->changeProxyStateMutex);
    this->BtoA_isClose = false;
    this->loadNum += 1;
    lock.unlock();
    char *buffer = nullptr;
    try
    {
        buffer = new char[this->buffer_size];
    }
    catch (const std::bad_alloc &e)
    {
        // 处理内存分配失败的情况
        std::cerr << "[ERROR]Memory allocation failed: " << e.what() << std::endl;
        goto _End;
    }
    // std::cout << "已创建" << this->connectB_ipv4 << "->" << this->connectA_ipv4 << "的映射\n";
    while (!this->isClose)
    {

        if (this->AtoB_isClose)
        {
            break;
        }

        struct pollfd pfd;
        pfd.fd = this->socket_connectB;
        pfd.events = POLLIN | POLLHUP | POLLERR;

        int ret = poll(&pfd, 1, 0);
        if (ret > 0)
        {
            if (pfd.revents & POLLHUP)
            {
                break;
            }
        }

        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(this->socket_connectB, SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0)
        {
            break;
        }

        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds);
        FD_ZERO(&exceptfds);
        FD_SET(this->socket_connectB, &readfds);
        FD_SET(this->socket_connectB, &exceptfds);

        struct timeval timeoutUse = this->timeout;

        int result = select(this->socket_connectB + 1, &readfds, NULL, &exceptfds, &timeoutUse);

        if (result < 0)
        {
            break;
        }

        usleep(1000);

        if (result == 0)
        {
            char buffer[1];
            ssize_t bytes_received = recv(this->socket_connectB, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);
            if (bytes_received == 0)
            {
                break;
            }
            else if (bytes_received == -1)
            {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                {
                    break;
                }
            }
        }

        if (FD_ISSET(this->socket_connectB, &readfds))
        {
            ssize_t recv_n = recv(this->socket_connectB, buffer, this->buffer_size, 0);
            if (recv_n <= 0)
            {
                break;
            }

            FD_ZERO(&writefds);
            FD_ZERO(&exceptfds);
            FD_SET(this->socket_connectA, &writefds);
            FD_SET(this->socket_connectA, &exceptfds);

            timeoutUse = this->timeout;

            result = select(this->socket_connectA + 1, NULL, &writefds, &exceptfds, &timeoutUse);
            if (result < 0)
            {
                break;
            }

            if (FD_ISSET(this->socket_connectA, &writefds))
            {
                size_t send_n = 0;
                while (send_n < recv_n)
                {
                    ssize_t sent = send(this->socket_connectA, buffer + send_n, recv_n - send_n, MSG_NOSIGNAL);
                    if (sent <= 0)
                    {
                        // 发送失败或连接关闭
                        break;
                    }
                    send_n += sent; // 累加已发送的字节数
                }
                lock.lock();
                Gloabl_global_info.recv_size += send_n;
                lock.unlock();
                // std::cout << this->connectB_ipv4 << "发送数据：" << this->connectA_ipv4 << " (" << send_n << "Bytes" << ")\n";

                if (send_n < recv_n)
                {
                    break;
                }
            }

            if (FD_ISSET(this->socket_connectA, &exceptfds))
            {
                break;
            }
        }

        if (FD_ISSET(this->socket_connectB, &exceptfds))
        {
            break;
        }
    }
    delete[] buffer;
_End:
    lock.lock();
    this->BtoA_isClose = true;
    lock.unlock();
    shutdown(this->socket_connectA, SHUT_WR);
    shutdown(this->socket_connectB, SHUT_RD);
    // std::cout << "已关闭" << this->connectB_ipv4 << "->" << this->connectA_ipv4 << "的映射\n";
}

void proxy_worker::setTimeout(int s, int us)
{
    this->timeout.tv_sec = s;
    this->timeout.tv_usec = us;
}

void proxy_worker::waitClose()
{
    while (true)
    {
        if (this->checkClose())
        {
            break;
        }
        sleep(1);
    }
}

bool proxy_worker::checkClose()
{
    if (this->loadNum == 2 && this->AtoB_isClose && this->BtoA_isClose)
    {
        return true;
    }
    return false;
}

void proxy_worker::forceClose()
{
    {
        std::unique_lock<std::mutex> lock(this->changeProxyStateMutex);
        this->BtoA_isClose = true;
        this->AtoB_isClose = true;
        this->loadNum = 2;
    }
    shutdown(this->socket_connectA, SHUT_WR);
    shutdown(this->socket_connectB, SHUT_RD);
}

proxy_worker::~proxy_worker() = default;
