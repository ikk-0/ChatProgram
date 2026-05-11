#include "server.h"
#include <signal.h>
using namespace std;

#define SER_PORT 8888
#define SER_IP "0.0.0.0" //192.168.233.129

TcpServer *g_server = nullptr;

void signalHandler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        cout << "\n收到停止信号,正在关闭服务器..." << endl;
        if (g_server)
        {
            g_server->Stop();
            exit(0);
        }
    }
}

int main(int argc, const char *argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    TcpServer server(SER_IP, SER_PORT);
    g_server = &server;

    if (!server.Init())
    {
        cerr << "Init failed" << endl;
        return -1;
    }
    if (!server.StartListen())
    {
        cerr << "StartListen failed" << endl;
        return -1;
    }
    if (!server.Run())
    {
        cerr << "Run failed" << endl;
        return -1;
    }

    return 0;
}