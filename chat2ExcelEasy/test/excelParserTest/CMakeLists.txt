cmake_minimum_required(VERSION 3.16)
project(ExcelParserTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)
find_package(OpenXLSX REQUIRED)
find_package(Threads REQUIRED)

# bite_scaffold 日志库（头文件位于 include/bite_scaffold 下）
find_path(BITE_SCAFFOLD_INCLUDE_DIR log.h PATHS /usr/include/bite_scaffold)
find_library(BITE_SCAFFOLD_LIBRARY NAMES bite_scaffold)

add_executable(excelParserTest
    excelParserTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source/execlParser.cc
)

target_include_directories(excelParserTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../source
    ${BITE_SCAFFOLD_INCLUDE_DIR}
)

# 测试Excel文件生成目录
target_compile_definitions(excelParserTest PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
)

target_link_libraries(excelParserTest PRIVATE
    GTest::gtest
    OpenXLSX::OpenXLSX
    ${BITE_SCAFFOLD_LIBRARY}
    spdlog
    fmt
    Threads::Threads
)

enable_testing()
add_test(NAME excelParserTest COMMAND excelParserTest)
