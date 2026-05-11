#include "server.h"
#include "protocol.h"
#include "database.h"
#include <vector>
using namespace std;

TcpServer::TcpServer(const std::string &ip, int port)
    : ip_(ip), port_(port), listen_fd(-1), epfd_(-1), is_running(false)
{
    cout << "TcpServer created" << endl;
}

TcpServer::~TcpServer()
{
    if (listen_fd != -1)
    {
        close(listen_fd);
        cout << "listen_fd closed" << endl;
    }
    if (epfd_ != -1)
    {
        close(epfd_);
        cout << "epfd_ closed" << endl;
    }
    cout << "TcpServer destroyed!" << endl;
}

int TcpServer::CreateSocket()
{
    // 创建套接字sfd
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
    {
        cerr << "socket error" << endl;
        return -1;
    }
    cout << "socket success! sfd = " << sfd << endl;
    return sfd;
}

bool TcpServer::BindInfo()
{
    /****************************************************/
    // 添加地址复用选项
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        cerr << "setsockopt error" << endl;
        // 不是致命错误，继续执行
    }
    /*当你的服务器程序异常退出（比如用 Ctrl+C 强制结束）时，操作系统会将端口锁定一段时间，
    通常为 2-4 分钟(防止旧连接的数据包干扰新连接)。在这段时间内，任何程序（包括重新启动的服务器）
    都无法绑定这个端口，导致 bind() 失败。*/
    /****************************************************/

    // 创建地址信息结构体
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_addr.s_addr = inet_addr(ip_.c_str()); // c_str() 获取 C 风格字符串
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port_);
    // 绑定ip和port
    if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        cerr << "bind error" << endl;
        return false;
    }
    cout << "bind success!" << endl;
    return true;
}

bool TcpServer::Init()
{
    listen_fd = CreateSocket();
    if (listen_fd == -1)
        return false;
    if (!BindInfo())
        return false;
    // 初始化数据库
    if (!InitDatabase())
    {
        cerr << "DataBase init failed" << endl;
        return false;
    }
    cout << "server information : "
         << "[" << ip_ << ":" << port_ << "]" << endl;
    return true;
}

bool TcpServer::InitDatabase()
{
    m_db = make_unique<DataBase>();
    return m_db->Init("localhost", "root", "1234", "chat", 3306);
}

void TcpServer::SendResponse(int fd, uint8_t msgType, const char *data, int len)
{
    // 消息格式：[4字节包体长度][1字节消息类型][消息体]
    int totalLen = 1 + len;
    char *buffer = new char[4 + totalLen];
    // 写入包体长度（网络字节序）
    uint32_t netLen = htonl(totalLen);
    memcpy(buffer, &netLen, 4);
    // 写入消息类型
    buffer[4] = msgType;
    // 写入消息体
    if (data && len > 0)
    {
        memcpy(buffer + 5, data, len);
    }

    send(fd, buffer, 4 + totalLen, 0);
    delete[] buffer;
}

void TcpServer::HandleLogin(int fd, char *data, int len)
{
    if (len < sizeof(LoginRequest))
    {
        cerr << "Login request too short" << endl;
        return;
    }
    LoginRequest *req = reinterpret_cast<LoginRequest *>(data);
    LoginResponse resp;

    // 检查用户是否在线
    if (m_userfds.find(req->username) != m_userfds.end())
    {
        resp.result = 3;
        strcpy(resp.message, "用户已经在其他地方登录");
        SendResponse(fd, MSG_LOGIN_RESP, (char *)&resp, sizeof(resp));
        return;
    }

    // 验证用户名密码
    if (m_db->CheckLogin(req->username, req->password))
    {
        // 用户密码正确
        resp.result = 0;
        strcpy(resp.message, "登录成功");

        // 记录用户在线状态
        m_userfds[req->username] = fd;
        m_clientUsers[fd] = req->username;
        m_db->SetUserOnline(req->username, 1);

        cout << "User logged in: " << req->username << " (fd=" << fd << ")" << endl;
    }
    else
    {
        // 错误
        resp.result = 2;
        strcpy(resp.message, "用户名或密码错误");
    }
    SendResponse(fd, MSG_LOGIN_RESP, (char *)&resp, sizeof(resp));
}

void TcpServer::HandleRegister(int fd, char *data, int len)
{
    if (len < sizeof(RegisterRequest))
    {
        cerr << "Register request too short" << endl;
        return;
    }
    RegisterRequest *req = reinterpret_cast<RegisterRequest *>(data);
    RegisterResponse resp;

    if (m_db->RegisterUser(req->username, req->password, req->nickname))
    {
        resp.result = 0;
        strcpy(resp.message, "注册成功");
        cout << "New user registered: " << req->username << endl;
    }
    else
    {
        resp.result = 1;
        strcpy(resp.message, "用户名已存在");
    }

    SendResponse(fd, MSG_REGISTER_RESP, (char *)&resp, sizeof(resp));
}

void TcpServer::HandleChat(int fd, char *data, int len)
{
    string sender = m_clientUsers[fd];
    if (sender.empty())
    {
        cerr << "Unknown sender" << endl;
        return;
    }
    // 简单回显（后续可以扩展为转发给其他用户）
    cout << "Chat from " << sender << ": " << string(data, len) << endl;

    // 回显消息给发送者
    SendResponse(fd, MSG_CHAT, data, len);
}

bool TcpServer::StartListen()
{
    // 监听客户端连接
    if (listen(listen_fd, 128) == -1)
    {
        cerr << "listen error" << endl;
        return false;
    }
    cout << "listen success!" << endl;
    return true;
}

void TcpServer::HandleClientInfo(int fd)
{
    char buffer[4096];
    int n = recv(fd, buffer, sizeof(buffer), 0);

    // 错误处理
    if (n <= 0)
    {
        if (n == 0)
        {
            cout << "Client disconnected (fd=" << fd << ")" << endl;
        }
        else
        {
            cerr << "recv error (fd=" << fd << ")" << endl;
        }
        RemoveClient(fd);
        return;
    }

    if (n < 5)
    {
        cerr << "Message too short" << endl;
        return;
    }

    // 解析消息头
    // 前4字节是包体长度（网络字节序）
    // 第5字节是消息类型
    // 读取 totalLen（包含类型字节的总长度）
    uint32_t totalLen = ntohl(*(uint32_t *)buffer);
    uint8_t msgType = buffer[4];

    // 实际数据长度 = totalLen - 1（减去类型字节）
    int dataLen = totalLen - 1;

    // 检查是否完整接收（4字节头 + totalLen）
    if (n < (int)(4 + totalLen))
    {
        cerr << "Incomplete message" << endl;
        return;
    }

    char *msgData = buffer + 5;

    switch (msgType)
    {
    case MSG_LOGIN_REQ:
        if (dataLen >= sizeof(LoginRequest))
        {
            HandleLogin(fd, msgData, dataLen);
        }
        break;
    case MSG_REGISTER_REQ:
        if (dataLen >= sizeof(RegisterRequest))
        {
            HandleRegister(fd, msgData, dataLen);
        }
        break;
    case MSG_CHAT:
        HandleChat(fd, msgData, dataLen);
        break;
    default:
        cerr << "Unknown message type: " << (int)msgType << endl;
        break;
    }
}

bool TcpServer::HandleNewConnection()
{
    struct sockaddr_in cin;
    socklen_t c_socklen = sizeof(cin);
    // 与客户端建立连接
    int newfd = accept(listen_fd, (struct sockaddr *)&cin, &c_socklen);
    if (newfd == -1)
    {
        cerr << "accept error" << endl;
        return false;
    }
    // 打印客户端IP和端口
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cin.sin_addr, ip, sizeof(ip));
    cout << "新客户端连接: " << ip << ":" << ntohs(cin.sin_port)
         << " [fd=" << newfd << "]" << endl;

    // 设置客户端 socket 为非阻塞模式
    int flags = fcntl(newfd, F_GETFL, 0);
    fcntl(newfd, F_SETFL, flags | O_NONBLOCK);

    // 将客户端文件描述符添加到集合中
    epoll_event ev;
    ev.data.fd = newfd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd_, EPOLL_CTL_ADD, newfd, &ev);
    return true;
}

void TcpServer::RemoveClient(int fd)
{
    auto it = m_clientUsers.find(fd);
    if (it != m_clientUsers.end())
    {
        string username = it->second;
        m_clientUsers.erase(fd);
        m_userfds.erase(username);
        if (m_db)
        {
            m_db->SetUserOnline(username, 0);
        }
        cout << "User offline: " << username << endl;
    }
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    cout << "移除客户端 [fd=" << fd << "]" << endl;
}

void TcpServer::Stop()
{
    is_running = false;

    // 清理所有已连接的客户端
    vector<int> fds_to_remove;
    for (auto &pair : m_clientUsers)
    {
        fds_to_remove.push_back(pair.first);
    }

    for (int fd : fds_to_remove)
    {
        RemoveClient(fd);
    }

    // 关闭监听 socket 和 epoll
    if (listen_fd != -1)
    {
        close(listen_fd);
        listen_fd = -1;
    }

    if (epfd_ != -1)
    {
        close(epfd_);
        epfd_ = -1;
    }

    cout << "Server stopped, all resources cleaned" << endl;
}

bool TcpServer::Run()
{
    // 创建epoll实例
    epfd_ = epoll_create(1);
    if (epfd_ == -1)
    {
        cerr << "epoll_create error" << endl;
        return false;
    }
    epoll_event ev;
    ev.data.fd = listen_fd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd_, EPOLL_CTL_ADD, listen_fd, &ev);

    // 创建处理事件集合
    epoll_event evs[1024];
    int evs_size = sizeof(evs) / sizeof(evs[0]);

    cout << "Server running... waiting for events" << endl;

    // 循环 连接 通讯
    is_running = true;
    while (is_running)
    {
        int num = epoll_wait(epfd_, evs, evs_size, -1);
        if (num == -1)
        {
            if (errno == EINTR)
                continue; // 被信号中断，继续
            cerr << "epoll_wait error: " << strerror(errno) << endl;
            break;
        }

        for (int i = 0; i < num; i++)
        {
            // 可能是sfd,也可能是newfd
            int fd = evs[i].data.fd; // 不需要判断 0、1、2，因为根本没添加到 epoll 中
            if (fd == listen_fd)
            {
                HandleNewConnection();
                continue;
            }
            else
            {
                HandleClientInfo(fd);
            }
        }
    }
    close(epfd_);
    return true;
}