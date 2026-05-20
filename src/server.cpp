#include "server.h"
#include "database.h"
#include "threadpool.h"
#include <vector>
#include <algorithm>
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
    
    // 创建线程池（4个工作线程）
    m_threadPool = make_unique<ThreadPool>(4);
    cout << "ThreadPool created with 4 workers" << endl;
    
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

// ========== 线程安全的用户表操作 ==========

void TcpServer::AddUser(int fd, const std::string& username)
{
    lock_guard<mutex> lock(m_userMutex);
    m_userfds[username] = fd;
    m_clientUsers[fd] = username;
}

void TcpServer::RemoveUser(int fd)
{
    lock_guard<mutex> lock(m_userMutex);
    auto it = m_clientUsers.find(fd);
    if (it != m_clientUsers.end()) {
        string username = it->second;
        m_userfds.erase(username);
        m_clientUsers.erase(it);
    }
}

bool TcpServer::IsUserOnline(const std::string& username)
{
    lock_guard<mutex> lock(m_userMutex);
    return m_userfds.find(username) != m_userfds.end();
}

int TcpServer::GetUserFd(const std::string& username)
{
    lock_guard<mutex> lock(m_userMutex);
    cout << "GetUserFd: looking for " << username << endl;  
    for (auto& pair : m_userfds) {
        cout << "  online: " << pair.first << " -> fd=" << pair.second << endl;  
    }
    auto it = m_userfds.find(username);
    if (it != m_userfds.end()) {
        cout << "  found: fd=" << it->second << endl; 
        return it->second;
    }
    cout << "  NOT found!" << endl; 
    return -1;
}

// ========== 业务处理函数（在工作线程中执行）==========

void TcpServer::HandleLogin(int fd, const char *data, int len)
{
    if (len < sizeof(LoginRequest))
    {
        cerr << "Login request too short" << endl;
        return;
    }
    const LoginRequest *req = reinterpret_cast<const LoginRequest *>(data);
    LoginResponse resp;

    // 检查用户是否在线（使用线程安全版本）
    if (IsUserOnline(req->username))
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

        // 记录用户在线状态（使用线程安全版本）
        AddUser(fd, req->username);
        m_db->SetUserOnline(req->username, 1);

        cout << "User logged in: " << req->username << " (fd=" << fd << ")" << endl;

        // 发送登录响应
        SendResponse(fd, MSG_LOGIN_RESP, (char *)&resp, sizeof(resp));
        
        // 发送当前用户列表给新登录的用户
        SendUserList(fd);
        
        // 通知其他用户有新用户上线
        NotifyUserOnline(req->username);
    }
    else
    {
        // 错误
        resp.result = 2;
        strcpy(resp.message, "用户名或密码错误");
        SendResponse(fd, MSG_LOGIN_RESP, (char *)&resp, sizeof(resp));
    }
}

void TcpServer::HandleRegister(int fd, const char *data, int len)
{
    if (len < sizeof(RegisterRequest))
    {
        cerr << "Register request too short" << endl;
        return;
    }
    const RegisterRequest *req = reinterpret_cast<const RegisterRequest *>(data);
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

void TcpServer::HandleChat(int fd, const char *data, int len)
{
    string sender;
    {
        lock_guard<mutex> lock(m_userMutex);
        auto it = m_clientUsers.find(fd);
        if (it != m_clientUsers.end()) {
            sender = it->second;
        }
    }
    
    if (sender.empty())
    {
        cerr << "Unknown sender" << endl;
        return;
    }
    // 简单回显
    cout << "Chat from " << sender << ": " << string(data, len) << endl;

    // 回显消息给发送者
    SendResponse(fd, MSG_CHAT, data, len);
}

void TcpServer::HandlePrivateChat(int fd, const char *data, int len)
{
    if (len < (int)sizeof(PrivateChat))
    {
        cerr << "Private chat message too short" << endl;
        return;
    }

    const PrivateChat* chat = reinterpret_cast<const PrivateChat*>(data);
    
    string sender;
    {
        lock_guard<mutex> lock(m_userMutex);
        auto it = m_clientUsers.find(fd);
        if (it != m_clientUsers.end()) {
            sender = it->second;
        }
    }
    
    string target = chat->to_user;

    cout << "Private chat from " << sender << " to " << target << ": " << chat->content << endl;

    cout << "Raw to_user: ";
    for (int i = 0; i < 32; i++) {
        cout << hex << (int)(unsigned char)chat->to_user[i] << " ";
    }
    cout << dec << endl;


    int targetFd = GetUserFd(target);
    if (targetFd != -1)
    {
        // 构建转发消息
        PrivateChat forwardChat;
        strncpy(forwardChat.from_user, sender.c_str(), 31);
        strncpy(forwardChat.to_user, target.c_str(), 31);
        strncpy(forwardChat.content, chat->content, 1023);
        
        SendResponse(targetFd, MSG_PRIVATE_CHAT, (char*)&forwardChat, sizeof(PrivateChat));
    }
    else
    {
        cout << "Target user " << target << " is offline" << endl;
    }
}

// ========== 用户列表相关函数 ==========

void TcpServer::BuildUserList(std::vector<UserInfo>& userList)
{
    lock_guard<mutex> lock(m_userMutex);
    userList.clear();
    for (auto &pair : m_userfds)
    {
        UserInfo info;
        strncpy(info.username, pair.first.c_str(), 31);
        strncpy(info.nickname, pair.first.c_str(), 31);
        info.status = 1;
        userList.push_back(info);
    }
}

void TcpServer::SendUserList(int fd)
{
    std::vector<UserInfo> userList;
    BuildUserList(userList);

    int dataLen = userList.size() * sizeof(UserInfo);
    char *data = new char[dataLen];

    for (size_t i = 0; i < userList.size(); i++)
    {
        memcpy(data + i * sizeof(UserInfo), &userList[i], sizeof(UserInfo));
    }
    SendResponse(fd, MSG_USER_LIST, data, dataLen);
    delete[] data;
}

void TcpServer::BroadcastUserList()
{
    std::vector<UserInfo> userList;
    BuildUserList(userList);

    int dataLen = userList.size() * sizeof(UserInfo);
    char *data = new char[dataLen];

    for (size_t i = 0; i < userList.size(); i++)
    {
        memcpy(data + i * sizeof(UserInfo), &userList[i], sizeof(UserInfo));
    }
    
    lock_guard<mutex> lock(m_userMutex);
    for (auto &pair : m_clientUsers)
    {
        SendResponse(pair.first, MSG_USER_LIST, data, dataLen);
    }

    delete[] data;
}

void TcpServer::NotifyUserOnline(const std::string &username)
{
    UserInfo info;
    strncpy(info.username, username.c_str(), 31);
    strncpy(info.nickname, username.c_str(), 31);
    info.status = 1;

    lock_guard<mutex> lock(m_userMutex);
    for (auto &pair : m_clientUsers)
    {
        if (pair.second != username)
        {
            SendResponse(pair.first, MSG_ONLINE_NOTIFY, (char *)&info, sizeof(UserInfo));
        }
    }
    cout << "Notified: user " << username << " online" << endl;
}

void TcpServer::NotifyUserOffline(const std::string &username)
{
    UserInfo info;
    strncpy(info.username, username.c_str(), 31);
    strncpy(info.nickname, username.c_str(), 31);
    info.status = 0;

    lock_guard<mutex> lock(m_userMutex);
    for (auto &pair : m_clientUsers)
    {
        SendResponse(pair.first, MSG_OFFLINE_NOTIFY, (char *)&info, sizeof(UserInfo));
    }
    cout << "Notified: user " << username << " offline" << endl;
}

// ========== 网络事件处理 ==========

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
     cout << "HandleClientInfo called for fd=" << fd << endl;
    
    char buffer[4096];
    int n = recv(fd, buffer, sizeof(buffer), 0);
    
    cout << "recv returned: " << n << ", errno=" << errno << endl; 
    
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

    // 解析消息头（快速操作，主线程做）
    uint32_t totalLen = ntohl(*(uint32_t *)buffer);
    uint8_t msgType = buffer[4];

    if (n < (int)(4 + totalLen))
    {
        cerr << "Incomplete message" << endl;
        return;
    }

    // 复制数据（因为 buffer 是局部变量，工作线程执行时可能已被覆盖）
    char* msgData = new char[totalLen - 1];
    memcpy(msgData, buffer + 5, totalLen - 1);
    int dataLen = totalLen - 1;

    // 提交到线程池处理业务逻辑
    m_threadPool->submit([this, fd, msgType, msgData, dataLen]() {
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
        case MSG_PRIVATE_CHAT:
            if (dataLen >= sizeof(PrivateChat))
            {
                HandlePrivateChat(fd, msgData, dataLen);
            }
            break;
        default:
            cerr << "Unknown message type: " << (int)msgType << endl;
            break;
        }
        delete[] msgData;  // 释放复制的数据
    });
    
    // 主线程立即返回，继续处理下一个事件
}

bool TcpServer::HandleNewConnection()
{
    struct sockaddr_in cin;
    socklen_t c_socklen = sizeof(cin);
    int newfd = accept(listen_fd, (struct sockaddr *)&cin, &c_socklen);
    if (newfd == -1)
    {
        cerr << "accept error"  << strerror(errno) << endl;
        return false;
    }
    
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cin.sin_addr, ip, sizeof(ip));
    cout << "新客户端连接: " << ip << ":" << ntohs(cin.sin_port)
         << " [fd=" << newfd << "]" << endl;

    // 设置客户端 socket 为非阻塞模式
    int flags = fcntl(newfd, F_GETFL, 0);
    fcntl(newfd, F_SETFL, flags | O_NONBLOCK);

    epoll_event ev;
    ev.data.fd = newfd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd_, EPOLL_CTL_ADD, newfd, &ev);
    return true;
}

void TcpServer::RemoveClient(int fd)
{
    // 获取用户名并移除
    string username;
    {
        lock_guard<mutex> lock(m_userMutex);
        auto it = m_clientUsers.find(fd);
        if (it != m_clientUsers.end())
        {
            username = it->second;
            m_clientUsers.erase(fd);
            
            if (m_db)
            {
                m_db->SetUserOnline(username, 0);
            }
        }
    }
    
    // 通知其他用户该用户下线
    if (!username.empty())
    {
        NotifyUserOffline(username);
        cout << "User offline: " << username << endl;
    }
    
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    cout << "移除客户端 [fd=" << fd << "]" << endl;
}

void TcpServer::Stop()
{
    is_running = false;
    
    // 停止线程池
    if (m_threadPool) {
        m_threadPool->stop();
    }

    // 清理所有已连接的客户端
    vector<int> fds_to_remove;
    {
        lock_guard<mutex> lock(m_userMutex);
        for (auto &pair : m_clientUsers)
        {
            fds_to_remove.push_back(pair.first);
        }
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

    is_running = true;
    while (is_running)
    {
        int num = epoll_wait(epfd_, evs, evs_size, -1);
        cout << "epoll_wait returned " << num << " events" << endl; 
        if (num == -1)
        {
            if (errno == EINTR)
                continue;
            cerr << "epoll_wait error: " << strerror(errno) << endl;
            break;
        }

        for (int i = 0; i < num; i++)
        {
            int fd = evs[i].data.fd;
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