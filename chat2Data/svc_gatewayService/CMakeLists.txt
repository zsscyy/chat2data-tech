# ==============================================================================
# 网关子服务 CMake构建配置文件
# ==============================================================================
#
# 项目名称：GatewayService
# 项目描述：基于cpp-httplib的网关HTTP服务器，负责前端请求接入与路由转发
#
# 依赖库：
#   - bite_scaffold：提供日志、服务发现（ETCD）、RPC信道管理、HTTP服务器等能力
#   - common：项目公共静态库（错误码、异常类、工具类）
#
# 构建步骤：
#   1. mkdir build && cd build
#   2. cmake ..
#   3. make
#
# 运行方式：
#   ./GatewayService [--gatewayPort=端口] [--etcdAddr=ETCD地址]
#
# ==============================================================================

# CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(GatewayService)

# C++标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 可执行文件定义
add_executable(${PROJECT_NAME}
    main.cc
    gatewayServer.cc
    gatewayServiceImpl.cc
)

# 可执行文件输出到构建目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# 头文件搜索路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 公共代码静态库（后续业务接口使用Chat2DataException时直接链接）
if(NOT TARGET common)
    add_library(common STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/errorHandler.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../common/utils.cc
    )
    target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../common)
endif()

# 查找bite_scaffold库（httplib、gflags、spdlog、jsoncpp、brpc、etcd等由它传递提供）
find_package(bite_scaffold REQUIRED)

# 链接库设置
target_link_libraries(${PROJECT_NAME} PRIVATE
    common
    bite_scaffold::bite_scaffold
)
