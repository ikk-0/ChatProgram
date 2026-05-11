#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <mutex>
#include <memory>

// 前向声明 MySQL 结构体
struct MYSQL;

class DataBase
{
public:
    DataBase();
    ~DataBase();

    bool Init(const std::string &host,
              const std::string &user,
              const std::string &password,
              const std::string &dbname,
              int port = 3306);

    bool CheckLogin(const std::string &username,
                    const std::string &password);
    bool RegisterUser(const std::string &username,
                      const std::string &password,
                      const std::string &nickname);

    bool UserExists(const std::string &username);
    std::string GetNickname(const std::string &username);
    bool SetUserOnline(const std::string &username, int status);
    
private:
    MYSQL* m_sql;  // MySQL 连接对象
    // 直接使用 MySQL 连接指针

    std::mutex m_mutex; // 多线程保护，同一时间只允许一个线程操作数据库
};

#endif