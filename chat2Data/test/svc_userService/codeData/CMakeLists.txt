# ==============================================================================
# 验证码数据访问层单元测试（独立工程，对齐老师写法）
#
# 两种构建方式：
#   1) 根目录：add_subdirectory(test/svc_userService/codeData)
#   2) 独立：cd test/svc_userService/codeData && mkdir build && cd build && cmake ..
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

project(CodeDataTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../)

add_executable(${PROJECT_NAME}
    codeDataTest.cc
    ${PROJECT_ROOT_DIR}/data/verifyCodeData.cc
    ${PROJECT_ROOT_DIR}/common/errorHandler.cc
)

find_package(GTest REQUIRED)
find_package(bite_scaffold REQUIRED)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_ROOT_DIR}/data
    ${PROJECT_ROOT_DIR}/data/odb
    ${PROJECT_ROOT_DIR}/svc_userService
    ${PROJECT_ROOT_DIR}/common
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::gtest
    GTest::gtest_main
    bite_scaffold::bite_scaffold
    redis++
    hiredis
    spdlog
    fmt
    jsoncpp
    pthread
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
