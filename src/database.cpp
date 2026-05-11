#include "database.h"
#include <mysql/mysql.h>
#include <iostream>
#include <cstring>

using namespace std;

DataBase::DataBase() : m_sql(nullptr) {};

DataBase::~DataBase()
{
    if (m_sql)
    {
        mysql_close(m_sql);
        m_sql = nullptr;
    }
};

bool DataBase::Init(const std::string &host,
                    const std::string &user,
                    const std::string &password,
                    const std::string &dbname,
                    int port)
{
    try
    {
        // 初始化 MySQL
        m_sql = mysql_init(nullptr);
        if (!m_sql)
        {
            cerr << "mysql_init failed" << endl;
            return false;
        }

        // 连接数据库
        if (!mysql_real_connect(m_sql, host.c_str(), user.c_str(), password.c_str(),
                                dbname.c_str(), port, nullptr, 0))
        {
            cerr << "mysql_real_connect failed: " << mysql_error(m_sql) << endl;
            mysql_close(m_sql);
            m_sql = nullptr;
            return false;
        }

        // 设置字符集为 UTF-8
        mysql_set_character_set(m_sql, "utf8mb4");

        // 创建用户表（如果不存在）
        const char *sql = R"(
            CREATE TABLE IF NOT EXISTS users(
                id INT PRIMARY KEY AUTO_INCREMENT,
                username VARCHAR(50) UNIQUE NOT NULL,
                password VARCHAR(255) NOT NULL,
                nickname VARCHAR(50) DEFAULT '用户',
                status INT DEFAULT 0,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";

        if (mysql_query(m_sql, sql))
        {
            cerr << "Create table failed: " << mysql_error(m_sql) << endl;
            return false;
        }

        cout << "Database connected successfully" << endl;
        return true;
    }
    catch (const exception &e)
    {
        cerr << "Database init error: " << e.what() << endl;
        return false;
    }
}

bool DataBase::UserExists(const std::string &username)
{
    // 注意：调用此函数前必须已持有锁，所以这里不加锁（避免死锁）
    
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM users WHERE username = '%s'",
             username.c_str());

    if (mysql_query(m_sql, sql))
    {
        cerr << "UserExists query failed: " << mysql_error(m_sql) << endl;
        return false;
    }

    MYSQL_RES *result = mysql_store_result(m_sql);
    if (!result)
    {
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int count = (row && row[0]) ? atoi(row[0]) : 0;
    mysql_free_result(result);

    return count > 0;
}

bool DataBase::CheckLogin(const std::string &username, const std::string &password)
{
    lock_guard<mutex> lock(m_mutex);

    try
    {
        char sql[512];
        snprintf(sql, sizeof(sql),
                 "SELECT COUNT(*) FROM users WHERE username = '%s' AND password = '%s'",
                 username.c_str(), password.c_str());

        if (mysql_query(m_sql, sql))
        {
            cerr << "CheckLogin query failed: " << mysql_error(m_sql) << endl;
            return false;
        }

        MYSQL_RES *result = mysql_store_result(m_sql);
        if (!result)
        {
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        int count = (row && row[0]) ? atoi(row[0]) : 0;
        mysql_free_result(result);

        if (count == 1)
        { // 登录成功，切换为在线状态
            char update_sql[256];
            snprintf(update_sql, sizeof(update_sql),
                     "UPDATE users SET status = 1 WHERE username = '%s'",
                     username.c_str());
            mysql_query(m_sql, update_sql);
            return true;
        }
        else
            return false;
    }
    catch (const exception &e)
    {
        cerr << "checklogin error: " << e.what() << endl;
        return false;
    }
}

bool DataBase::RegisterUser(const std::string &username, const std::string &password, const std::string &nickname)
{
    lock_guard<mutex> lock(m_mutex);

    try
    {
        if (UserExists(username))
        {
            return false;
        }
        char sql[1024];
        snprintf(sql, sizeof(sql),
                 "INSERT INTO users (username,password,nickname,status)"
                 "VALUES ('%s', '%s', '%s', 0)",
                 username.c_str(), password.c_str(), nickname.c_str());

        if (mysql_query(m_sql, sql))
        {
            cerr << "RegisterUser failed: " << mysql_error(m_sql) << endl;
            return false;
        }
        return true;
    }
    catch (const exception &e)
    {
        cerr << "registerUser error: " << e.what() << endl;
        return false;
    }
}

std::string DataBase::GetNickname(const std::string &username)
{
    lock_guard<mutex> lock(m_mutex);

    try
    {
        char sql[256];
        snprintf(sql, sizeof(sql),
                 "SELECT nickname FROM users WHERE username = '%s'",
                 username.c_str());

        if (mysql_query(m_sql, sql))
        {
            return "";
        }

        MYSQL_RES *result = mysql_store_result(m_sql);
        if (!result)
        {
            return "";
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        string nickname = (row && row[0]) ? row[0] : "";
        mysql_free_result(result);

        return nickname;
    }
    catch (const exception &e)
    {
        cerr << "getNickname error: " << e.what() << endl;
        return "";
    }
}

bool DataBase::SetUserOnline(const std::string &username, int status)
{
    lock_guard<mutex> lock(m_mutex);

    try
    {
        char sql[256];
        snprintf(sql, sizeof(sql),
                 "UPDATE users SET status = %d WHERE username = '%s'",
                 status, username.c_str());

        if (mysql_query(m_sql, sql))
        {
            cerr << "setUserOnline error: " << mysql_error(m_sql) << endl;
            return false;
        }
        return true;
    }
    catch (const exception &e)
    {
        cerr << "setUserOnline error: " << e.what() << endl;
        return false;
    }
}