#include "networker.h"
#include "client.h"
#include "protocol.h"
#include <QDebug>
#include <QCoreApplication>
#include <cstring>
#include <iostream>
using namespace std;

NetWorker::NetWorker(QObject *parent)
    : QObject(parent)
    , m_tid(0)
    , m_client(nullptr)
    , m_isRunning(false)
    , m_shouldStop(false)
{
}

NetWorker::~NetWorker()
{
    Stop();
    if (m_tid) {
        pthread_join(m_tid, NULL);  // 等待线程结束
    }
}

void NetWorker::Start(const std::string& ip, int port)
{
    m_client = std::make_unique<Client>();
    m_client->SetServerInfo(ip,port);

    if(pthread_create(&m_tid,NULL,ThreadFunc,this) != 0)
    {
        emit errorOccurred("创建线程失败");
        return;
    }
}

void NetWorker::Stop()
{
    m_shouldStop = true;
    m_isRunning = false;
}

void NetWorker::Run()
{
    //初始化并连接
    if(!m_client->Init())
    {
        emit errorOccurred("初始化失败");
        return;
    }
    if(!m_client->Connect())
    {
        emit errorOccurred("连接失败");
        return;
    }
    emit connected();
    m_isRunning = true;

    char buffer[4096];
    while(m_isRunning && !m_shouldStop)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(m_client->GetSocket(),&fds);
        struct timeval tv = {0,100};       // 100ms超时

        // 使用 select 实现超时接收
        int ret = select(m_client->GetSocket()+1,&fds,NULL,NULL,&tv);
        if(ret > 0)
        {
            int res = m_client->Recv(buffer,sizeof(buffer));
            if(res>0)
            {
                buffer[res] = '\0';

                // 解析消息
                if(res<5) continue;
                uint32_t totalLen = ntohl(*(uint32_t*)buffer);
                uint8_t msgType = buffer[4];
                if(res < (int)(4 + totalLen)) continue;

                char* msgData = buffer + 5;
                int dataLen = totalLen - 1;

                switch(msgType)
                {
                case MSG_LOGIN_RESP:{
                    if (dataLen >= sizeof(LoginResponse)) {
                        LoginResponse* resp = reinterpret_cast<LoginResponse*>(msgData);
                        emit loginResponse(resp->result, QString::fromUtf8(resp->message));
                    }
                    break;
                }
                case MSG_REGISTER_RESP:{
                    if (dataLen >= sizeof(RegisterResponse)) {
                        RegisterResponse* resp = reinterpret_cast<RegisterResponse*>(msgData);
                        emit registerResponse(resp->result, QString::fromUtf8(resp->message));
                    }
                    break;
                }
                case MSG_CHAT:{
                    QString msg = QString::fromUtf8(msgData, dataLen);
                    emit messageReceived(msg);
                    break;
                }
                default:
                    break;
                }
            }
            else if(res == 0)  // 对端关闭连接
            {
                emit disconnected();
                break;
            }
            else
            {
                int err = WSAGetLastError();
                if(err != WSAEWOULDBLOCK)   // WSAEWOULDBLOCK 无数据可读，忽略
                {
                    emit errorOccurred("接收数据失败");
                    break;
                }
            }
        }
        QCoreApplication::processEvents();
    }
    if (m_client) {
        m_client->DisConnect();
    }
    emit disconnected();
}

void NetWorker::SendMessage_(uint8_t msgType, const char* data, int len)
{
    if (!m_client || !m_client->IsConnected()) {
        emit errorOccurred("未连接到服务器");
        return;
    }
    int totalLen=1+len;

    char* buffer=new char[4+totalLen];

    uint32_t netLen = htonl(totalLen);
    memcpy(buffer,&netLen,4);
    buffer[4]=msgType;
    if(data&&len>0)
    {
        memcpy(buffer+5,data,len);
    }
    m_client->Send(buffer,4+totalLen);
    delete[] buffer;
}

void* NetWorker::ThreadFunc(void* arg)
{
    NetWorker* work = static_cast<NetWorker*>(arg);
    work->Run();
    return nullptr;
}

void NetWorker::SendLogin(const std::string& username, const std::string& password)
{
    LoginRequest req;
    strncpy(req.username,username.c_str(),31);
    strncpy(req.password,password.c_str(),31);
    SendMessage_(MSG_LOGIN_REQ,reinterpret_cast<char*>(&req),sizeof(req));
}

void NetWorker::SendRegister(const std::string& username, const std::string& password, const std::string& nickname)
{
    RegisterRequest req;
    strncpy(req.username,username.c_str(),31);
    strncpy(req.password,password.c_str(),31);
    strncpy(req.nickname,nickname.c_str(),31);

    SendMessage_(MSG_REGISTER_REQ,reinterpret_cast<char*>(&req),sizeof(req));
}

void NetWorker::SendChat(const std::string& msg)
{
    SendMessage_(MSG_CHAT,msg.c_str(),msg.size());
}