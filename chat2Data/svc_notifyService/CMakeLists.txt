# ==============================================================================
# 通知子服务构建配置
# ==============================================================================
#
# 可执行文件：NotifyService
#
# 依赖：common、proto、bite_scaffold（brpc/etcd/curl/spdlog/gflags）
#
# 构建方式：
#   1) 推荐在 chat2Data 父目录构建：cmake -S . -B build && cmake --build build
#   2) 也可在本目录独立构建：cmake -S . -B build && cmake --build build
#
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

project(NotifyService)

# 独立构建时，父工程未引入 common/proto，需要在此补齐，
# 否则 errorHandler.h 等公共头文件找不到，且 common/proto 等库目标不存在
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common ${CMAKE_CURRENT_BINARY_DIR}/common)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../proto ${CMAKE_CURRENT_BINARY_DIR}/proto)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
    main.cc
    emailSender.cc
    verifyCodeEmailSender.cc
    normalEmailSender.cc
    emailWorker.cc
    notifyBusiness.cc
    notifyServiceImpl.cc
    notifyServer.cc
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 查找bite_scaffold库（brpc、etcd、curl、spdlog、gflags等由它传递提供）
find_package(bite_scaffold REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    common
    proto
    bite_scaffold::bite_scaffold
    curl
    pthread
)
