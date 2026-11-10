# ==============================================================================
# 浼氳瘽鏁版嵁璁块棶灞傚崟鍏冩祴璇曪紙鐙珛宸ョ▼锛屽榻愯€佸笀鍐欐硶锛?#
# 涓ょ鏋勫缓鏂瑰紡锛?#   1) 鏍圭洰褰曪細add_subdirectory(test/svc_userService/sessionData)
#   2) 鐙珛锛歝d test/svc_userService/sessionData && mkdir build && cd build && cmake ..
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

project(SessionDataTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../)

add_executable(${PROJECT_NAME}
    sessionDataTest.cc
    ${PROJECT_ROOT_DIR}/data/sessionData.cc
    ${PROJECT_ROOT_DIR}/data/userEntity.cc
    ${PROJECT_ROOT_DIR}/data/sessionEntity.cc
    ${PROJECT_ROOT_DIR}/data/odb/userEntity-odb.cxx
    ${PROJECT_ROOT_DIR}/data/odb/sessionEntity-odb.cxx
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
