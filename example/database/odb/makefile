# 编译主程序和ODB生成的代码，链接相关库
# -lodb-mysql: ODB MySQL 支持库
# -lodb: ODB 核心库
# -lbite_scaffold: 项目封装的 ODB 工具库
# -I.: 包含当前目录头文件（student.hpp 等）
# -I./build: 包含 build 目录下的 ODB 生成头文件
main : main.cc build/student-odb.cxx
	g++ main.cc build/student-odb.cxx -o main -lodb-mysql -lodb -lbite_scaffold -std=c++17 -I. -I./build

# ODB 代码生成规则：根据 student.hpp 生成数据库操作代码和 SQL 文件
# 输出：student-odb.cxx、student-odb.hxx、student.sql
build/student-odb.cxx build/student-odb.hxx build/student.sql : student.hpp
# 创建 build 目录（如果不存在）
	@mkdir -p build

# 调用 ODB 编译器解析 student.hpp 中的 #pragma db 注解
# -d mysql: 指定目标数据库为 MySQL
# --std c++17: 生成 C++17 兼容代码
# --generate-query: 生成查询支持代码（支持 odb::query）
# --generate-schema: 生成数据库 schema SQL 文件
	@odb -d mysql --std c++17 --generate-query --generate-schema student.hpp

# 将生成的文件移动到 build 目录 
	@mv student-odb.* build/
	@mv student.sql build/

clean:
	@rm -rf main build
