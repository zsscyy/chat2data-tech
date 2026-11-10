# ==============================================================================
# 用户子服务构建配置
# ==============================================================================
#
# 可执行文件：UserService
#
# 依赖：common、proto、dataAccess、dataEntity、bite_scaffold（brpc/etcd/redis/spdlog/gflags）
#
# 构建方式（在 chat2Data 父目录执行）：
#   mkdir build && cd build
#   cmake .. && make
#
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

project(UserService)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
    main.cc
    sessionManager.cc
    userBusiness.cc
    userServiceImpl.cc
    userServer.cc
    userServerBuilder.cc
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 查找bite_scaffold库（brpc、etcd、redis++、spdlog、gflags等由它传递提供）
find_package(bite_scaffold REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    common
    proto
    dataAccess
    dataEntity
    bite_scaffold::bite_scaffold
    odb
    odb-mysql
    crypt
    pthread
)
