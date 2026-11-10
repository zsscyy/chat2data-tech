# 设置项目名称、设置C++标准、设置编译类型、添加需要编译的文件、链接需要的库

# 项目名称为AIChatDemo
project(AIChatDemo)

# 设置C++标准，使用C++17标准进行编译，REQUIRED表示编译器如果不支持C++17则报错
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型为Debug
set(CMAKE_BUILD_TYPE Debug)

# 添加可执行文件
add_executable(AIChatDemo chatsdkDemo.cc)

# 查找OpenSSL包
find_package(OpenSSL REQUIRED)

# 将 OpenSSL 的头文件目录添加到编译器的搜索路径中
include_directories(${OPENSSL_INCLUDE_DIR})

# 设置库的目录，链接器会在这个目录中查找需要的库文件ai_chat_sdk库文件
link_directories(/usr/local/lib)

# 链接SDK
target_link_libraries(AIChatDemo PRIVATE
    ai_chat_sdk
    fmt
    jsoncpp
    OpenSSL::SSL
    OpenSSL::Crypto
    gflags
    spdlog
    sqlite3
)