# ==============================================================================
# 数据实体层构建配置（ODB 代码生成 + 实体静态库）
# ==============================================================================
#
# 说明：
#   1. 自动扫描本目录下所有 *Entity.h 实体头文件，逐个调用 odb 生成映射代码
#   2. 生成的 .hxx/.cxx/.ixx 输出到本目录 odb 子目录
#   3. 生成的建表 .sql 输出到本目录 sql 子目录
#
# 注意：新增实体头文件后需重新执行 cmake，使其被 glob 纳入构建。
#
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

# 支持两种构建方式：
#   1) 根目录构建：由 chat2Data/CMakeLists.txt 通过 add_subdirectory(data) 引入，
#      下面的独立构建分支不会执行，依赖由父工程统一提供；
#   2) 独立构建：在 data/build 中执行 cmake ..，此时需要自己声明 project()
#      并引入 ../common，才能拿到 errorHandler.h / utils.h 等公共头文件。
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(chat2DataData LANGUAGES CXX)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common ${CMAKE_CURRENT_BINARY_DIR}/common)
endif()

find_program(ODB_EXECUTABLE odb REQUIRED)

set(DATA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ODB_OUTPUT_DIR ${DATA_SOURCE_DIR}/odb)
set(SQL_OUTPUT_DIR ${DATA_SOURCE_DIR}/sql)

file(MAKE_DIRECTORY ${ODB_OUTPUT_DIR})
file(MAKE_DIRECTORY ${SQL_OUTPUT_DIR})

# 自动收集本目录下的实体头文件与实现文件（命名约定：*Entity.h / *Entity.cc）
file(GLOB ENTITY_HEADERS "${DATA_SOURCE_DIR}/*Entity.h")
file(GLOB ENTITY_SOURCES "${DATA_SOURCE_DIR}/*Entity.cc")
if(NOT ENTITY_HEADERS)
    message(FATAL_ERROR "在 ${DATA_SOURCE_DIR} 下未找到任何 *Entity.h 实体头文件")
endif()
if(NOT ENTITY_SOURCES)
    message(FATAL_ERROR "在 ${DATA_SOURCE_DIR} 下未找到任何 *Entity.cc 实体实现文件")
endif()

set(ODB_GENERATED_FILES)
foreach(ENTITY_HEADER ${ENTITY_HEADERS})
    # 实体名 = 去掉目录与扩展名的文件名，例如 userEntity
    get_filename_component(ENTITY_NAME ${ENTITY_HEADER} NAME_WE)
    set(GENERATED_HEADER ${ODB_OUTPUT_DIR}/${ENTITY_NAME}-odb.hxx)
    set(GENERATED_SOURCE ${ODB_OUTPUT_DIR}/${ENTITY_NAME}-odb.cxx)
    set(GENERATED_INTERFACE ${ODB_OUTPUT_DIR}/${ENTITY_NAME}-odb.ixx)
    set(GENERATED_SQL ${SQL_OUTPUT_DIR}/${ENTITY_NAME}.sql)

    # 生成映射代码（.hxx/.cxx/.ixx），输出到 odb 目录
    add_custom_command(
        OUTPUT ${GENERATED_HEADER} ${GENERATED_SOURCE}
        BYPRODUCTS ${GENERATED_INTERFACE}
        COMMAND ${ODB_EXECUTABLE}
                -d mysql
                --std c++17
                -I ${DATA_SOURCE_DIR}
                --generate-query
                -o ${ODB_OUTPUT_DIR}
                ${ENTITY_HEADER}
        DEPENDS ${ENTITY_HEADER}
        COMMENT "Generating ODB mapping code for ${ENTITY_NAME}"
    )

    # 生成建表 SQL（.sql），输出到 sql 目录
    add_custom_command(
        OUTPUT ${GENERATED_SQL}
        COMMAND ${ODB_EXECUTABLE}
                -d mysql
                --std c++17
                -I ${DATA_SOURCE_DIR}
                --generate-schema-only
                --schema-format sql
                -o ${SQL_OUTPUT_DIR}
                ${ENTITY_HEADER}
        DEPENDS ${ENTITY_HEADER}
        COMMENT "Generating ODB schema SQL for ${ENTITY_NAME}"
    )

    list(APPEND ODB_GENERATED_FILES ${GENERATED_HEADER} ${GENERATED_SOURCE})
    list(APPEND ODB_SCHEMA_FILES ${GENERATED_SQL})
endforeach()

# 建表 SQL 不参与库编译，但需随构建一并生成
add_custom_target(dataSchema ALL DEPENDS ${ODB_SCHEMA_FILES})

add_library(dataEntity STATIC
    ${ENTITY_SOURCES}
    ${ODB_GENERATED_FILES}
)
target_include_directories(dataEntity PUBLIC ${DATA_SOURCE_DIR} ${ODB_OUTPUT_DIR})
target_link_libraries(dataEntity PUBLIC odb-mysql odb)
set_target_properties(dataEntity PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# 数据访问层静态库（用户/会话/验证码的 CURD 与缓存读写）
# ==============================================================================
add_library(dataAccess STATIC
    userData.cc
    sessionData.cc
    verifyCodeData.cc
)
target_include_directories(dataAccess PUBLIC ${DATA_SOURCE_DIR})
target_link_libraries(dataAccess PUBLIC
    dataEntity
    common
    redis++
    hiredis
    spdlog
    fmt
)
set_target_properties(dataAccess PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
