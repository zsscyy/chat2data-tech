# ==============================================================================
# Excel解析器单元测试（独立工程，也可由父工程 add_subdirectory 引入）
#
# 构建方式：
#   1) 根目录：add_subdirectory(test/svc_excelParserService/excelParserTest)
#   2) 独立：cd test/svc_excelParserService/excelParserTest && mkdir build && cd build && cmake ..
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

project(ExcelParserTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../)

add_executable(${PROJECT_NAME}
    excelParserTest.cc
    ${PROJECT_ROOT_DIR}/svc_excelParserService/excelParser.cc
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

find_package(GTest REQUIRED)
find_package(bite_scaffold REQUIRED)
find_package(OpenXLSX REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_ROOT_DIR}/svc_excelParserService
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    GTest::gtest
    bite_scaffold::bite_scaffold
    OpenXLSX::OpenXLSX
    pthread
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
