#include "headfile.h"

std::vector<std::pair<socklen_t, sockaddr_in>> connectPool_Global;
std::mutex connectPoolMutex;
std::mutex proxyChangeMutex;
ServerInfo *pServerInfo_Global;
ThreadpoolAutoCtrlByTime *threadpool;

proxy_global_info Gloabl_global_info;
bool proxy_sever_close = false;
int proxy_server_ctrl_type = 0;

int auto_info_time;
int server_port;
int thread_num;
int keep_tread_num;
int buffer_size;
std::string client_ip;
int client_port;
int timeoutConf;
std::vector<std::string> ban_ip;
std::vector<std::string> allow_ip;

struct proxy_info
{
    ClientInfo *socketA;
    ClientInfo *socketB;
    char *aIpv4;
    char *bIpv4;
    void *Worker;
};

std::vector<proxy_info *> proxyInfo_Pool;

bool isIpMatched(const std::string &ip)
{
    // 检查ban_ip列表中是否有通配符"*"
    bool hasWildcardBan = std::find(ban_ip.begin(), ban_ip.end(), "*") != ban_ip.end();

    if (hasWildcardBan)
    {
        // 如果ban_ip中有"*"，则只允许在allow_ip列表中的IP访问
        if (std::find(allow_ip.begin(), allow_ip.end(), ip) != allow_ip.end() ||
            std::find(allow_ip.begin(), allow_ip.end(), "*") != allow_ip.end())
        {
            return true; // IP被明确允许或允许所有
        }
        else
        {
            return false; // IP被禁止（因为不在allow_ip列表中）
        }
    }
    else
    {
        // 如果ban_ip中没有"*"，首先检查是否在ban列表中
        if (std::find(ban_ip.begin(), ban_ip.end(), ip) != ban_ip.end())
        {
            return false; // IP被禁止
        }

        // 如果不在ban列表中，默认允许，但优先考虑allow列表
        if (std::find(allow_ip.begin(), allow_ip.end(), ip) != allow_ip.end() ||
            std::find(allow_ip.begin(), allow_ip.end(), "*") != allow_ip.end())
        {
            return true; // IP被明确允许或允许所有
        }

        // 默认行为: 如果没有明确禁止且不在allow列表中，默认允许
        return true;
    }
}

std::string format_size(size_t size)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;

    double formatted_size = static_cast<double>(size);

    while (formatted_size >= 1024 && unit_index < 4)
    {
        formatted_size /= 1024;
        unit_index++;
    }

    char buffer[50];
    snprintf(buffer, sizeof(buffer), "%.2f %s", formatted_size, units[unit_index]);

    return std::string(buffer);
}

void run_proxy_a(void *proxy_worker_class)
{
    std::unique_lock<std::mutex> lock(proxyChangeMutex);
    Gloabl_global_info.use_thread_number++;
    Gloabl_global_info.use_socket_number++;
    lock.unlock();
    proxy_worker *Worker = (proxy_worker *)proxy_worker_class;
    Worker->controlAtoB();
    lock.lock();
    Gloabl_global_info.use_socket_number--;
    Gloabl_global_info.use_thread_number--;
    lock.unlock();
}

void run_proxy_b(void *proxy_worker_class)
{
    std::unique_lock<std::mutex> lock(proxyChangeMutex);
    Gloabl_global_info.use_thread_number++;
    Gloabl_global_info.use_socket_number++;
    lock.unlock();
    proxy_worker *Worker = (proxy_worker *)proxy_worker_class;
    Worker->controlBtoA();
    lock.lock();
    Gloabl_global_info.use_socket_number--;
    Gloabl_global_info.use_thread_number--;
    lock.unlock();
}

void push_connectPool_Global(socklen_t socketField, sockaddr_in sockaddrIpv4)
{
    std::lock_guard<std::mutex> lock(connectPoolMutex);
    connectPool_Global.emplace_back(socketField, sockaddrIpv4);
}

void remove_connectPool_Global_bySocketField(socklen_t socketField, bool isClose = false)
{
    std::lock_guard<std::mutex> lock(connectPoolMutex);
    auto newEnd = std::remove_if(connectPool_Global.begin(), connectPool_Global.end(),
                                 [socketField, isClose](const std::pair<socklen_t, sockaddr_in> &element) // 修正捕获列表
                                 {
                                     if (element.first == socketField)
                                     {
                                         if (isClose)
                                         {
                                             close(element.first);
                                         }
                                         return true;
                                     }
                                     return false;
                                 });
    connectPool_Global.erase(newEnd, connectPool_Global.end());
}

void close_proxy_socket_make_sure(struct proxy_info *proxyInfo)
{
    if (!proxyInfo)
        return;

    auto close_socket = [](ClientInfo *sock)
    {
        if (!sock)
            return;

        int fd = sock->socketFD;
        if (fd < 0)
            return;

        // 先执行 shutdown，确保 TCP 关闭正确
        shutdown(fd, SHUT_RDWR);

        // 适当调整 linger 选项，或者不使用它
        struct linger lopt = {1, 1}; // 等待 1 秒，确保数据发送
        setsockopt(fd, SOL_SOCKET, SO_LINGER, &lopt, sizeof(lopt));

        // 关闭连接
        CloseConnect(sock); // 确保这个函数真的 close(fd)
        FreeClient(sock);
    };

    close_socket(proxyInfo->socketA);
    proxyInfo->socketA = nullptr;

    close_socket(proxyInfo->socketB);
    proxyInfo->socketB = nullptr;
}

void create_proxy_worker(void *date)
{
    std::unique_lock<std::mutex> lock(proxyChangeMutex);
    if (proxy_server_ctrl_type != 1)
    {
        Gloabl_global_info.use_thread_number++;
    }
    lock.unlock();

    struct proxy_info *proxyInfo = (struct proxy_info *)date;
    proxy_worker *Worker = new proxy_worker;
    char *aIpv4 = GetConnectIPV4(&proxyInfo->socketA->serverAddr);
    char *bIpv4 = GetConnectIPV4(&proxyInfo->socketB->serverAddr);
    Worker->connectA_ipv4 = aIpv4;
    Worker->connectB_ipv4 = bIpv4;
    Worker->buffer_size = buffer_size;
    Worker->setTimeout(timeoutConf, 0);
    Worker->submitSocketField(proxyInfo->socketA->socketFD, proxyInfo->socketB->socketFD);
    proxyInfo->aIpv4 = aIpv4;
    proxyInfo->bIpv4 = bIpv4;
    proxyInfo->Worker = (void *)Worker;
    threadpool->submitMission(run_proxy_a, (void *)Worker);
    threadpool->submitMission(run_proxy_b, (void *)Worker);
    if (proxy_server_ctrl_type == 1)
    {
        lock.lock();
        proxyInfo_Pool.push_back(proxyInfo);
        lock.unlock();
    }
    else
    {
        Worker->waitClose();
        remove_connectPool_Global_bySocketField(proxyInfo->socketA->socketFD);
        remove_connectPool_Global_bySocketField(proxyInfo->socketB->socketFD);
        close_proxy_socket_make_sure(proxyInfo);
        FreeConnectIPV4String(aIpv4);
        FreeConnectIPV4String(bIpv4);
        delete proxyInfo;
        delete Worker;
        lock.lock();
        Gloabl_global_info.use_thread_number--;
        lock.unlock();
    }
}

void check_proxy_worker_close_thread()
{
    while (!proxy_sever_close)
    {
        auto removal_predicate = [](proxy_info *proxyInfo)
        {
            // 检查指针是否为空
            if (!proxyInfo || !proxyInfo->Worker || !proxyInfo->socketA || !proxyInfo->socketB)
            {
                return false; // 跳过无效的 proxyInfo
            }

            proxy_worker *worker = static_cast<proxy_worker *>(proxyInfo->Worker);

            // 检查是否需要关闭
            if (!worker->checkClose())
            {
                return false; // 不需要关闭
            }

            // 清理资源
            remove_connectPool_Global_bySocketField(proxyInfo->socketA->socketFD, false);
            remove_connectPool_Global_bySocketField(proxyInfo->socketB->socketFD, false);

            // 关闭 socket 并释放资源
            close_proxy_socket_make_sure(proxyInfo);
            FreeConnectIPV4String(proxyInfo->aIpv4);
            FreeConnectIPV4String(proxyInfo->bIpv4);

            // 释放 worker 和 proxyInfo
            delete worker;
            proxyInfo->Worker = nullptr; // 避免悬空指针
            delete proxyInfo;

            return true;
        };

        // 加锁保护 proxyInfo_Pool 的访问
        std::unique_lock<std::mutex> lock(proxyChangeMutex);

        // 使用 remove_if 和 erase 清理需要关闭的 proxyInfo
        proxyInfo_Pool.erase(
            std::remove_if(proxyInfo_Pool.begin(), proxyInfo_Pool.end(),
                           [&](proxy_info *proxyInfo)
                           {
                               return removal_predicate(proxyInfo);
                           }),
            proxyInfo_Pool.end());

        lock.unlock();

        // 调整休眠时间以提高性能
        usleep(10 * 1000); // 10 毫秒
    }
}

void ConnectMissionDrop(const std::vector<std::any> &args)
{
    if (!args.empty())
    {
        try
        {
            // 从 any 中取出 void*
            void *rawPtr = std::any_cast<void *>(args[0]);

            // 再转成你需要的类型
            proxy_worker *proxyWorker = static_cast<proxy_worker *>(rawPtr);

            proxyWorker->forceClose();

            // {
            //     std::unique_lock<std::mutex> lock(proxyChangeMutex);
            //     Gloabl_global_info.use_thread_number--;
            //     delete proxyInfo;
            // }

            std::cout << "任务被丢弃";
        }
        catch (const std::bad_any_cast &e)
        {
            std::cout << "[类型错误: " << e.what() << "]";
        }
    }

    std::cout << std::endl;
}

bool HandleServerConnect(int clientFD, struct sockaddr_in clientAddr)
{
    if (proxy_sever_close)
    {
        return true;
    }
    char *ipv4 = GetConnectIPV4(&clientAddr);

    if (ipv4 != NULL && isIpMatched(ipv4))
    {
        struct proxy_info *proxyInfo = new proxy_info;
        memset(proxyInfo, 0, sizeof(struct proxy_info));
        proxyInfo->socketB = InitClient();
        if (proxyInfo->socketB != NULL)
        {
            SetClientConnectTimeout(proxyInfo->socketB, timeoutConf, 0);
            if (ConnectServer(proxyInfo->socketB, (char *)client_ip.c_str(), client_port, false) == 0)
            {
                proxyInfo->socketA = CreateClient(clientFD, clientAddr);
                if (proxyInfo->socketA != NULL)
                {
                    proxyInfo->socketA->isExist = true;
                    push_connectPool_Global(clientFD, clientAddr);
                    push_connectPool_Global(proxyInfo->socketB->socketFD, proxyInfo->socketB->serverAddr);
                    // std::cout << "[INFO]建立转发：" << ipv4 << " <-> " << client_ip << std::endl;
                    if (proxy_server_ctrl_type == 1)
                    {
                        create_proxy_worker(proxyInfo);
                    }
                    else
                    {
                        threadpool->submitMission(create_proxy_worker, (void *)proxyInfo);
                    }
                }
                else
                {
                    std::cout << "[ERROR]内存申请失败（" << ipv4 << "已断开）" << std::endl;
                    FreeClient(proxyInfo->socketB);
                    delete proxyInfo;
                    return true;
                }
            }
            else
            {
                std::cout << "[ERROR]连接远程服务器失败（" << ipv4 << "已断开）" << std::endl;
                FreeClient(proxyInfo->socketB);
                FreeConnectIPV4String(ipv4);
                delete proxyInfo;
                return true;
            }
        }
    }
    else if (ipv4 != NULL)
    {
        std::cout << "[WARN]拒绝IP：" << ipv4 << std::endl;
        FreeConnectIPV4String(ipv4);
        return true;
    }
    else
    {
        // 处理 ipv4 == NULL 的情况
        std::cout << "[WARN]获取客户端IP失败" << std::endl;
        FreeConnectIPV4String(ipv4);
        return true;
    }

    FreeConnectIPV4String(ipv4);
    return false;
}

void auto_info()
{
    auto last_info_time = std::chrono::steady_clock::now(); // 记录上次执行 info 的时间
    while (!proxy_sever_close)
    {
        // 检查是否达到 10 分钟
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_info_time).count() >= auto_info_time)
        {
            // 执行 info 操作
            std::cout << "=================================" << std::endl;
            std::cout << "自动执行 info：" << std::endl;
            std::cout << "发送数据: " << format_size(Gloabl_global_info.send_size) << std::endl;
            std::cout << "接收数据: " << format_size(Gloabl_global_info.recv_size) << std::endl;
            std::cout << "当前使用线程数：" << Gloabl_global_info.use_thread_number << std::endl;
            std::cout << "当前套接字数：" << Gloabl_global_info.use_socket_number << std::endl;
            std::cout << "当前连接数：" << connectPool_Global.size() << std::endl;
            std::cout << "=================================" << std::endl;

            // 更新上次执行 info 的时间
            last_info_time = now;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void listen_server()
{
    ListenServer(pServerInfo_Global, 0);
}

void SigintHandler(int sig)
{
    if (sig == SIGINT && !proxy_sever_close)
    {
        std::cout << "[INFO] 正在关闭存在的套接字" << std::endl;

        // 关闭监听socket，让accept失败
        if (pServerInfo_Global && pServerInfo_Global->socketFD >= 0)
        {
            CloseServer(pServerInfo_Global);
        }

        proxy_sever_close = true;
    }
}

int main()
{

    struct sigaction sa;
    sa.sa_handler = SigintHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    // 添加 SA_RESTART 标志让系统调用在信号处理后自动重启
    // sa.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        std::cerr << "[WARN] 信号处理函数初始化失败" << std::endl;
    }

    try
    {
        // 加载YAML文件
        YAML::Node config = YAML::LoadFile("./config.yml");

        int add_thread_number = 2;

        // 访问数据
        auto_info_time = config["server"]["autoInfoTime"].as<int>();
        server_port = config["server"]["port"].as<int>();
        thread_num = config["server"]["threadNumber"].as<int>();
        keep_tread_num = config["server"]["keepThreadNumber"].as<int>();
        proxy_server_ctrl_type = config["server"]["closeModleType"].as<int>();
        buffer_size = config["server"]["bufferSize"].as<int>();
        timeoutConf = config["server"]["timeout"].as<int>();
        client_ip = config["client"]["ip"].as<std::string>();
        client_port = config["client"]["port"].as<int>();

        YAML::Node banList = config["ip"]["ban"];
        YAML::Node allowList = config["ip"]["allow"];

        if (auto_info_time > 0)
        {
            add_thread_number++;
        }

        threadpool = new ThreadpoolAutoCtrlByTime(keep_tread_num, thread_num + add_thread_number);
        threadpool->setMissionDropCallback(ConnectMissionDrop);
        threadpool->openOutputError();

        std::cout << "Ban List:" << std::endl;
        for (const auto &item : banList)
        {
            if (item.IsScalar())
            { // 检查是否为标量值
                std::string value = item.as<std::string>();
                ban_ip.push_back(value);
                std::cout << "- " << value << std::endl;
            }
        }

        std::cout << "Allow List:" << std::endl;
        for (const auto &item : allowList)
        {
            if (item.IsScalar())
            { // 检查是否为标量值
                std::string value = item.as<std::string>();
                allow_ip.push_back(value);
                std::cout << "- " << value << std::endl;
            }
        }

        std::cout << "Choose Model";

        if (proxy_server_ctrl_type == 1)
        {
            std::cout << "OneThreadCtrl";
        }
        else
        {
            std::cout << "CreateThreadCtrl";
        }
        std::cout << std::endl;
    }
    catch (const YAML::Exception &e)
    {
        std::cerr << "YAML Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    pServerInfo_Global = InitServer();
    if (pServerInfo_Global == NULL)
    {
        std::cout << "[ERROR]初始化服务器失败" << std::endl;
        return EXIT_FAILURE;
    }
    while (CreateServer(pServerInfo_Global, server_port, true) != 0)
    {
        std::cout << "[INFO]尝试启动服务器" << std::endl;
        sleep(1);
    }
    std::cout << "[INFO]服务器启动成功" << std::endl;

    threadpool->submitMission(listen_server);

    if (auto_info_time > 0)
        threadpool->submitMission(auto_info);

    if (proxy_server_ctrl_type == 1)
    {
        threadpool->submitMission(check_proxy_worker_close_thread);
    }

    std::string input;

    while (true)
    {
        // 检查用户输入
        if (std::getline(std::cin, input))
        {
            if (input == "stop")
            {
                proxy_sever_close = true;
                CloseServer(pServerInfo_Global);

                std::cout << "[INFO] 正在关闭存在的套接字" << std::endl;

                break; // 退出循环
            }
            else if (input == "info")
            {
                std::cout << "发送数据: " << format_size(Gloabl_global_info.send_size) << std::endl;
                std::cout << "接收数据: " << format_size(Gloabl_global_info.recv_size) << std::endl;
                std::cout << "当前使用线程数：" << Gloabl_global_info.use_thread_number << std::endl;
                std::cout << "当前套接字数：" << Gloabl_global_info.use_socket_number << std::endl;
                std::cout << "当前连接数：" << connectPool_Global.size() << std::endl;
            }
        }

        // 防止过快轮询输入
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 等待一段时间确保listen_server有足够时间响应并退出
    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (auto &socketInfo : connectPool_Global)
    {
        int sockfd = socketInfo.first; // 获取套接字描述符
        if (sockfd >= 0)
        { // 确保套接字描述符有效
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);         // 关闭套接字
            socketInfo.first = -1; // 标记为已关闭或者无效
        }
    }

    // 清理资源
    FreeServer(pServerInfo_Global);
    threadpool->shutdown();

    delete threadpool;

    return EXIT_SUCCESS;
}
