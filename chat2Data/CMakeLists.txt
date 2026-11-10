# ==============================================================================
# chat2Data 项目父级构建配置
# ==============================================================================
#
# 统一组织 common、proto 两个公共库，以及 data 数据访问代码的构建。
#
# 构建方式：
#   mkdir build && cd build
#   cmake ..
#   make
#
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

project(chat2Data LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_subdirectory(common)
add_subdirectory(proto)
add_subdirectory(data)
add_subdirectory(svc_userService)

# 单元测试：直连 dev-mysql / dev-redis，通过 ctest 运行
add_subdirectory(test/svc_userService/userData)
add_subdirectory(test/svc_userService/sessionData)
add_subdirectory(test/svc_userService/codeData)
