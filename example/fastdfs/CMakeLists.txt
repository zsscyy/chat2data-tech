# 1. 声明cmake所需版本
cmake_minimum_required(VERSION 3.1.3)

# 2. 设置C++标准，使用C++17标准进行编译，REQUIRED表示编译器如果不支持C++17则报错
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3. 设置工程项目名称（内部会生成一系列的内置变量）
project(main VERSION 1.0)

# 4. 设置编译类型
set(CMAKE_BUILD_TYPE Debug)

# 5. 添加生成目标（可执行程序）
add_executable(${PROJECT_NAME} main.cc)

# 6. 依赖检查（依赖库的查找以及链接选项的添加）
find_package(bite_scaffold REQUIRED)

# 7. 设置链接库
target_link_libraries(${PROJECT_NAME} PRIVATE bite_scaffold::bite_scaffold)