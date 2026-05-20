cmake_minimum_required(VERSION 3.10)
project(ChatServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 指定MySQL路径
set(MYSQL_INCLUDE_DIR "/usr/include/mysql")
set(MYSQL_LIBRARY_DIR "/usr/lib64/mysql")

# 头文件路径
include_directories(include ${MYSQL_INCLUDE_DIR})

# 库文件路径
link_directories(${MYSQL_LIBRARY_DIR})

# 生成可执行程序
add_executable(chat_server
    main.cpp
    src/server.cpp
    src/database.cpp
    src/threadpool.cpp
)

# 链接库
target_link_libraries(chat_server 
    pthread
    mysqlclient
)