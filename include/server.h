#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>  
#include <errno.h>
#include <signal.h>
#include <map>
#include <memory>

//// 前向声明，避免在头文件中包含过多依赖
class DataBase;

class TcpServer
{
private:
    int port_;
    std::string ip_;
    int listen_fd; // sfd
    int epfd_;
    bool is_running;

    std::unique_ptr<DataBase> m_db;       // 数据库对象
    std::map<int,std::string> m_clientUsers;   // 快速查找 fd -> 用户名
    std::map<std::string,int> m_userfds;       // 快速查找 用户名 -> fd

public:
    TcpServer(const std::string &ip, int port);
    ~TcpServer();

    bool Init();
    bool InitDatabase();
    bool StartListen();
    bool Run();
    void Stop();

private:
    int CreateSocket();
    bool BindInfo();

    void HandleClientInfo(int fd);
    bool HandleNewConnection();
    void RemoveClient(int fd);

    void HandleLogin(int fd, char *data, int len);
    void HandleRegister(int fd, char *data, int len);
    void HandleChat(int fd, char *data, int len);

    void SendResponse(int fd, uint8_t msgType, const char *data, int len);

    void BroadcastUserList();                           // 广播用户列表给所有在线用户
    void SendUserList(int fd);                          // 发送用户列表给指定客户端
    void NotifyUserOnline(const std::string& username); // 通知所有用户某人上线
    void NotifyUserOffline(const std::string& username);// 通知所有用户某人下线
    void HandlePrivateChat(int fd, char* data, int len);// 处理私聊消息
    void BuildUserList(std::vector<UserInfo>& userList); // 构建用户列表
};

#endif