# ==============================================================================
# 文件子服务纯工具函数单元测试
#
# 两种构建方式：
#   1) 根目录：add_subdirectory(test/svc_fileService/fileUtils)
#   2) 独立：cd test/svc_fileService/fileUtils && mkdir build && cd build && cmake ..
# 说明：不依赖 MySQL / Redis，只校验文件后缀判断与表名生成规则
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

project(FileUtilsTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../)

add_executable(${PROJECT_NAME}
    fileUtilsTest.cc
    ${PROJECT_ROOT_DIR}/svc_fileService/common.cc
    ${PROJECT_ROOT_DIR}/common/utils.cc
)

find_package(GTest REQUIRED)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_ROOT_DIR}/svc_fileService
    ${PROJECT_ROOT_DIR}/common
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::gtest
    GTest::gtest_main
    pthread
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
