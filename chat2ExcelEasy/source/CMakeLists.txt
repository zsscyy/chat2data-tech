# ==============================================================================
# Chat2ExcelEasy HTTP服务器 CMake构建配置文件
# ==============================================================================
# 
# 项目名称：chat2ExcelServer
# 项目描述：基于cpp-httplib的HTTP服务器，用于Excel数据分析助手系统
# 
# 依赖库：
#   - bite_scaffold：提供日志、JSON序列化、文件读写等功能
#   - cpp-httplib：HTTP服务器库（通过bite_scaffold间接依赖）
#   - gflags：命令行参数解析（通过bite_scaffold间接依赖）
#   - spdlog：日志库（通过bite_scaffold间接依赖）
# 
# 构建步骤：
#   1. mkdir build && cd build
#   2. cmake ..
#   3. make
# 
# 运行方式：
#   ./chat2ExcelServer [--host=地址] [--port=端口]
# 
# ==============================================================================

# ------------------------------------------------------------------------------
# CMake最低版本要求
# ------------------------------------------------------------------------------
# 设置CMake的最低版本为3.10，确保支持现代CMake特性
cmake_minimum_required(VERSION 3.10)

# ------------------------------------------------------------------------------
# 项目配置
# ------------------------------------------------------------------------------
# 定义项目名称为chat2ExcelServer（小写，与bite_scaffold要求一致）
# 注意：项目名称必须与add_executable中的目标名称一致
project(chat2ExcelServer)

# ------------------------------------------------------------------------------
# C++标准设置
# ------------------------------------------------------------------------------
# 设置C++标准为C++17，项目使用C++17特性（如std::optional、结构化绑定等）
set(CMAKE_CXX_STANDARD 17)
# 强制要求使用C++17，如果编译器不支持则报错
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 设置构建类型为Debug，包含调试信息，方便开发调试
set(CMAKE_BUILD_TYPE Debug)

# ------------------------------------------------------------------------------
# 可执行文件定义
# ------------------------------------------------------------------------------
# 定义可执行文件目标，使用${PROJECT_NAME}确保与项目名称一致
# 源文件列表：
#   - main.cc：主程序入口，负责服务器启动和运行
#   - chat2ExcelServer.cc：HTTP服务器实现
#   - execlParser.cc：Excel文件解析器实现
add_executable(${PROJECT_NAME}
    main.cc
    chat2ExcelServer.cc
    execlParser.cc
)

# ------------------------------------------------------------------------------
# 输出路径设置
# ------------------------------------------------------------------------------
# 设置可执行文件的输出路径为构建目录
# 这样生成的可执行文件位于build/目录下，便于管理和分发
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# ------------------------------------------------------------------------------
# 头文件目录设置
# ------------------------------------------------------------------------------
# 添加当前源码目录到头文件搜索路径
# 这样可以使用#include "chat2ExcelServer.h"而不是绝对路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ------------------------------------------------------------------------------
# 依赖库查找
# ------------------------------------------------------------------------------
# 查找OpenXLSX库（必需）
# OpenXLSX是一个用于读写Excel文件的C++库
find_package(OpenXLSX REQUIRED)

# 查找bite_scaffold库（必需）
# bite_scaffold是一个综合性的C++微服务脚手架库，提供：
#   - 日志系统（基于spdlog）
#   - JSON序列化（基于jsoncpp）
#   - HTTP服务器（基于cpp-httplib）
#   - 命令行参数解析（基于gflags）
#   - 其他常用工具
# 
# 注意：bite_scaffold会自动查找并链接其依赖的所有库
find_package(bite_scaffold REQUIRED)

# 查找OpenSSL库（必需）
# ChatSDK依赖OpenSSL进行HTTPS通信
find_package(OpenSSL REQUIRED)

# 查找ai_chat_sdk（provider已按Gitee仓库替换为GLM/MiniMax）
# 无sudo权限时SDK安装在用户级前缀 ~/.local，也兼容 /usr/local 和自定义前缀
find_path(AI_CHAT_SDK_INCLUDE_DIR
    NAMES ai_chat_sdk/ChatSDK.h
    HINTS ${AI_CHAT_SDK_ROOT} $ENV{HOME}/.local /usr/local
    PATH_SUFFIXES include
)
find_library(AI_CHAT_SDK_LIBRARY
    NAMES ai_chat_sdk
    HINTS ${AI_CHAT_SDK_ROOT} $ENV{HOME}/.local /usr/local
    PATH_SUFFIXES lib
)

if(NOT AI_CHAT_SDK_INCLUDE_DIR OR NOT AI_CHAT_SDK_LIBRARY)
    message(FATAL_ERROR "ai_chat_sdk not found, please install SDK first")
endif()

message(STATUS "ai_chat_sdk include: ${AI_CHAT_SDK_INCLUDE_DIR}")
message(STATUS "ai_chat_sdk library: ${AI_CHAT_SDK_LIBRARY}")

target_include_directories(${PROJECT_NAME} PRIVATE ${AI_CHAT_SDK_INCLUDE_DIR})

# ------------------------------------------------------------------------------
# 链接库设置
# ------------------------------------------------------------------------------
# 将依赖库链接到可执行文件
# 使用PRIVATE表示该库仅用于当前目标，不传递给依赖此目标的其他目标
# 
# bite_scaffold会自动链接以下库：
#   - httplib（cpp-httplib）
#   - gflags
#   - spdlog
#   - jsoncpp
#   - pthread
#   - 以及其他传递依赖
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenXLSX::OpenXLSX
    bite_scaffold::bite_scaffold
    ${AI_CHAT_SDK_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
    sqlite3
)

# 定义httplib中的CPPHTTPLIB_OPENSSL_SUPPORT宏
# 启用HTTPS支持
target_compile_definitions(${PROJECT_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
