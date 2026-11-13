# ==============================================================================
# Proto 接口代码生成与静态库构建配置
# ==============================================================================
#
# 说明：
#   1. 自动扫描本目录下全部 *.proto，逐个用 protoc 生成 .pb.h / .pb.cc
#   2. 生成结果统一输出到本目录的 protoCode 子目录
#   3. 所有生成文件打包为静态库 proto，供各子服务链接
#
# 新增、删除、重命名 proto 文件后，直接重新执行 cmake --build/make 即可，
# 无需修改本文件，也无需手动重跑 cmake：CONFIGURE_DEPENDS 会在构建时
# 自动检测目录变化并重新扫描。
#
# ==============================================================================
cmake_minimum_required(VERSION 3.12)

find_program(PROTOC_EXECUTABLE protoc REQUIRED)

set(PROTO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROTO_CODE_DIR ${PROTO_SOURCE_DIR}/protoCode)

file(MAKE_DIRECTORY ${PROTO_CODE_DIR})

# 扫描全部 proto 文件；CONFIGURE_DEPENDS 保证新增文件无需手动重新配置
file(GLOB PROTO_FILES CONFIGURE_DEPENDS ${PROTO_SOURCE_DIR}/*.proto)

set(PROTO_GENERATED_FILES)
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_FILE_NAME ${PROTO_FILE} NAME)
    string(REGEX REPLACE "\\.proto$" "" PROTO_NAME ${PROTO_FILE_NAME})
    set(PROTO_GENERATED_HEADER ${PROTO_CODE_DIR}/${PROTO_NAME}.pb.h)
    set(PROTO_GENERATED_SOURCE ${PROTO_CODE_DIR}/${PROTO_NAME}.pb.cc)

    # 依赖全部 proto 文件：任一 proto 变化（含被 import 的公共 proto）都会重新生成
    add_custom_command(
        OUTPUT ${PROTO_GENERATED_HEADER} ${PROTO_GENERATED_SOURCE}
        COMMAND ${PROTOC_EXECUTABLE}
                --proto_path=${PROTO_SOURCE_DIR}
                --cpp_out=${PROTO_CODE_DIR}
                ${PROTO_FILE}
        DEPENDS ${PROTO_FILES}
        COMMENT "Generating protobuf code for ${PROTO_NAME}.proto"
        VERBATIM
    )

    list(APPEND PROTO_GENERATED_FILES ${PROTO_GENERATED_HEADER} ${PROTO_GENERATED_SOURCE})
endforeach()

if(NOT PROTO_GENERATED_FILES)
    message(FATAL_ERROR "在 ${PROTO_SOURCE_DIR} 下未找到任何 .proto 文件")
endif()

add_library(proto STATIC ${PROTO_GENERATED_FILES})
target_include_directories(proto PUBLIC ${PROTO_CODE_DIR} ${PROTO_SOURCE_DIR})
target_link_libraries(proto PUBLIC protobuf)
set_target_properties(proto PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
