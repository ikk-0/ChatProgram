#ifndef NETWORKER_H
#define NETWORKER_H

#include <QObject>
#include <pthread.h>
#include <string.h>
#include <memory>


class Client;

class NetWorker : public QObject
{
    Q_OBJECT
public:
    explicit NetWorker(QObject *parent = nullptr);
    ~NetWorker();

    void Start(const std::string& ip, int port);
    void Stop();

    void SendLogin(const std::string& username, const std::string& password);
    void SendRegister(const std::string& username, const std::string& password, const std::string& nickname);
    void SendChat(const std::string& msg);;

signals:
    void connected();                              // 连接成功
    void disconnected();                           // 断开连接
    void loginResponse(int result, const QString& message);
    void registerResponse(int result, const QString& message);
    void messageReceived(const QString &msg);      // 收到消息
    void errorOccurred(const QString &error);      // 错误

private:
    static void* ThreadFunc(void* arg);     // 线程入口函数
    void Run();     // 线程主进程
    void SendMessage_(uint8_t msgType, const char* data, int len);

    pthread_t m_tid;
    std::unique_ptr<Client> m_client;
    bool m_isRunning;
    bool m_shouldStop;

};

#endif // NETWORKER_H
