# ==============================================================================
# 公共代码静态库构建配置
# ==============================================================================
cmake_minimum_required(VERSION 3.10)

if(NOT TARGET common)
    add_library(common STATIC
        errorHandler.cc
        utils.cc
    )
    target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(common PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
endif()
