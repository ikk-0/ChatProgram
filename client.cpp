#include "client.h"
#include <iostream>
using namespace std;

Client::Client()
    : winsock_started(false),port_(8888),connect_fd(INVALID_SOCKET)
{}

Client::~Client()
{
    DisConnect();
}

void Client::SetServerInfo(const std::string& ip, int port)
{
    ip_ = ip;
    port_ = port;
}

bool Client::Init()
{
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup error" << endl;
        return false;
    }

    winsock_started = true;

    //创建套接字
    connect_fd = socket(AF_INET,SOCK_STREAM,0);
    if(connect_fd == INVALID_SOCKET)
    {
        cerr<<"socket error"<<endl;
        DisConnect();
        return false;
    }
    cout<<"socket success, fd = "<<connect_fd<<endl;

    //设置非阻塞模式
    u_long mode = 1;
    ioctlsocket(connect_fd,FIONBIO,&mode);

    return true;
}

bool Client::Connect()
{
    //绑定信息
    struct sockaddr_in sin;
    memset(&sin,0,sizeof(sin));
    sin.sin_addr.s_addr = inet_addr(ip_.c_str());
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port_);

    cout << "Connecting to " << ip_ << ":" << port_ << endl;

    //发起连接（非阻塞）
    int con_ret =connect(connect_fd,(struct sockaddr*)&sin,sizeof(sin));
    if (con_ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        // 非阻塞模式下，connect 不会立刻连接成功，会返回 WSAEWOULDBLOCK
        if (err != WSAEWOULDBLOCK) {
            cerr << "connect error: " << err << endl;
            DisConnect();
            return false;
        }
    }

    //select等待连接完成。防止一直阻塞
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(connect_fd,&writefds);
    struct timeval tv = {5, 0};  // 5秒超时

    int sel_ret = select(connect_fd+1,NULL,&writefds,NULL,&tv);
    if (sel_ret <= 0) {
        if (sel_ret == 0) {
            cerr << "connect timeout" << endl;
        } else {
            cerr << "select error: " << WSAGetLastError() << endl;
        }
        DisConnect();
        return false;
    }

    //检查连接是否成功
    int err = 0;
    int len = sizeof(err);
    getsockopt(connect_fd, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) {
        cerr << "connect failed after select: " << err << endl;
        DisConnect();
        return false;
    }

    cout << "Connected to server successfully!" << endl;

    return true;
}

void Client::DisConnect()
{
    if(connect_fd != INVALID_SOCKET)
    {
        closesocket(connect_fd);
        connect_fd = INVALID_SOCKET;
    }
    if (winsock_started) {
        WSACleanup();
        winsock_started = false;
    }
}

bool Client::IsConnected() const
{
    return connect_fd != INVALID_SOCKET;
}

int Client::Send(const char* data,int len)
{
    if (!IsConnected()) {
        return SOCKET_ERROR;
    }
    return send(connect_fd,data,len,0);
}

int Client::Recv(char* buffer, int buffer_size)
{
    if (!IsConnected()) {
        return SOCKET_ERROR;
    }
    return recv(connect_fd,buffer,buffer_size,0);
}
SOCKET Client::GetSocket() const
{
    return connect_fd;
}