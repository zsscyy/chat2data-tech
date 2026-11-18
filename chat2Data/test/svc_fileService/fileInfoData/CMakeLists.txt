# ==============================================================================
# 文件信息数据访问层单元测试
#
# 两种构建方式：
#   1) 根目录：add_subdirectory(test/svc_fileService/fileInfoData)
#   2) 独立：cd test/svc_fileService/fileInfoData && mkdir build && cd build && cmake ..
# 说明：直接编译数据层源码，不依赖父工程的 dataAccess/dataEntity target
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

project(FileInfoDataTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../)

add_executable(${PROJECT_NAME}
    fileInfoDataTest.cc
    ${PROJECT_ROOT_DIR}/data/fileInfoData.cc
    ${PROJECT_ROOT_DIR}/data/fileInfoEntity.cc
    ${PROJECT_ROOT_DIR}/data/worksheetEntity.cc
    ${PROJECT_ROOT_DIR}/data/odb/fileInfoEntity-odb.cxx
    ${PROJECT_ROOT_DIR}/data/odb/worksheetEntity-odb.cxx
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
    ${PROJECT_ROOT_DIR}/svc_fileService
    ${PROJECT_ROOT_DIR}/common
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::gtest
    GTest::gtest_main
    bite_scaffold::bite_scaffold
    odb
    odb-mysql
    mysqlclient
    redis++
    hiredis
    spdlog
    fmt
    jsoncpp
    pthread
)

if(TARGET dataEntity)
    add_dependencies(${PROJECT_NAME} dataEntity)
endif()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
