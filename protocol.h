#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>

// 消息类型定义
enum MsgType : uint8_t {
    MSG_LOGIN_REQ = 0x01,      // 登录请求
    MSG_LOGIN_RESP = 0x02,     // 登录响应
    MSG_REGISTER_REQ = 0x03,   // 注册请求
    MSG_REGISTER_RESP = 0x04,  // 注册响应
    MSG_CHAT = 0x05,           // 聊天消息（群聊/保留）
    MSG_HEARTBEAT = 0x06,      // 心跳包
    MSG_USER_LIST = 0x10,      // 用户列表同步
    MSG_ONLINE_NOTIFY = 0x11,  // 用户上线通知
    MSG_OFFLINE_NOTIFY = 0x12, // 用户下线通知
    MSG_PRIVATE_CHAT = 0x13    // 私聊消息
};

// 登录请求包体
struct LoginRequest {
    char username[32];
    char password[32];

    LoginRequest() {
        memset(username, 0, sizeof(username));
        memset(password, 0, sizeof(password));
    }
};

// 登录响应包体
struct LoginResponse {
    int result;     // 0:成功, 1:密码错误, 2:用户不存在, 3:已在线
    char message[64];

    LoginResponse() : result(0) {
        memset(message, 0, sizeof(message));
    }
};

// 注册请求包体
struct RegisterRequest {
    char username[32];
    char password[32];
    char nickname[32];

    RegisterRequest() {
        memset(username, 0, sizeof(username));
        memset(password, 0, sizeof(password));
        memset(nickname, 0, sizeof(nickname));
    }
};

// 注册响应包体
struct RegisterResponse {
    int result;     // 0:成功, 1:用户名已存在, 2:其他错误
    char message[64];

    RegisterResponse() : result(0) {
        memset(message, 0, sizeof(message));
    }
};

// 用户信息结构体
struct UserInfo {
    char username[32];
    char nickname[32];
    int status;     // 0:离线, 1:在线

    UserInfo() : status(0) {
        memset(username, 0, sizeof(username));
        memset(nickname, 0, sizeof(nickname));
    }
};

// 私聊消息结构体
struct PrivateChat {
    char from_user[32];
    char to_user[32];
    char content[1024];

    PrivateChat() {
        memset(from_user, 0, sizeof(from_user));
        memset(to_user, 0, sizeof(to_user));
        memset(content, 0, sizeof(content));
    }
};

#endif