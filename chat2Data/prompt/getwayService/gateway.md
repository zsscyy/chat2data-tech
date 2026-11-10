
**系统身份**：你是一个资深的C++后端开发工程师，对微服务架构有深入了解，擅长网关设计和开发

**任务**：帮我给chat2Data项目搭建网关服务，需要实现HTTP服务器、HTTP接口定义(业务功能暂不实现)以及路由绑定、并完成main方法实现。

**任务拆分**
请按照以下步骤实现：

1. 先熟悉chat2data-tech/chat2Data/svc_gatewayService/API.md文件接口定义
2. 使用cpp-httplib库搭建网关服务器，网关的地址和端口通过gflags参数配置，gflag参数配置在main.cc中定义
3. 实现HTTP接口定义，总共30个HTTP接口，仅健康检测接口需要实现，其余接口只定义不实现(注意服务器搭建和HTTP接口实现隔离开，在不同类中实现)
4. 实现网关服务器构建器(Builder模式)，完成服务器(IP和端口)以及服务发现组件的构建
5. 实现main方法，完成参数解析、日志初始化 以及 HTTP服务器启动
6. 编写CMakeLists.txt文件，完成网关代码的构建

**网关服务器实现要求**

- 仅搭建HTTP服务器，不负责业务逻辑，业务逻辑在其他类中实现
- 使用cpp-httplib库搭建，网关地址和端口通过gflags参数配置
- 提供start方法：服务器启动时需检测服务器是否已运行，如果运行则返回，未运行再启动
- 提供stop方法：服务器停止时需检测服务器是否在运行，如果未运行则返回，运行中则停止。stop在服务器对象销毁时自动调用
- 服务器在新线程中启动，防止阻塞主线程
- 各个方法在实现时，必须要添加日志记录，记录程序的执行流程、状态以及异常情况
- 服务器中需要包含接口定义实例和服务器实例，在构造方法中完成初始化
- 服务器类名称GatewayServer，包含在GatewayService命名空间中
- 头文件名称gatewayServer.h，源文件名称gatewayServer.cc
- 网关服务器搭建，可参考chat2data-tech/chat2ExcelEasy/source中的chat2ExcelServer.h、chat2ExcelServer.cc文件

**HTTP接口实现要求**

- HTTP接口定义和HTTP服务器搭建分离开，在不同类中实现
- 请严格遵循chat2data-tech/chat2Data/svc_gatewayService/API.md文件接口定义，完成30个HTTP接口的定义和路由绑定
- 仅健康检测接口需要实现，其余接口只定义不实现
- 向外提供路由绑定接口，能让网关服务器在启动前完成路由绑定
- 需要包含服务器发现实例，能让后续各个接口实现时，获取到RPC服务器Channel
- 类名称GatewayServiceImpl，包含在GatewayService命名空间中
- 头文件名称gatewayServiceImpl.h，源文件名称gatewayServiceImpl.cc
- HTTP接口定义和路由绑定，可参考chat2data-tech/chat2ExcelEasy/source中的chat2ExcelServer.h、chat2ExcelServer.cc文件
  注意务必确保HTTP接口定义符合chat2data-tech/chat2Data/svc_gatewayService/API.md文件接口定义

**网关服务器构建器(Builder模式)实现要求**

- 职责完成网关服务器构建，完成服务器发现组件构建
- 构建器流程如下：
  1. 构建服务信道管理对象，并设置需要监控的服务
  2. 构造服务发现(监控)对象，定义回调函数，处理服务上线、下线时，服务节点的添加和移除，并打印日志记录
  3. 在独立的线程中启动服务监控
  4. 构建HTTP接口定义对象
  5. 构建cpp-httplib的HTTP服务器对象，并设置读写超时时间为5分钟
  6. 绑定路由
  7. 构建网关服务器对象
  8. 启动服务器
  9. 返回网关服务器对象
- 构建器类名称GatewayServerBuilder，和网关服务器放在一个文件中实现

**main.cc实现要求**

1. 定义gflags参数，包括:
   - bitelog日志配置
   - 网关服务器端口9000
   - ETCD地址http://dev-etcd:2379
   - 服务名称：用户子服务-UserSerivce、文件子服务-FileService、数据库子服务-DataBaseService、AI子服务-AIService
2. 定义SIGINT和SIGTERM信号处理函数，用于处理程序退出信号，程序退出时如果服务器在运行则停止服务器，并打印日志记录
3. main方法的实现流程：
   a. 解析gflags参数
   b. 初始化日志记录
   c. 注册信号处理函数，处理SIGINT和SIGTERM信号
   d. 配置服务发现，监控用户子服务、文件子服务、数据库子服务、AI子服务的上线、下线事件
   d. 构建网关服务器对象
   e. 启动服务器
   f. 循环检测程序退出信号，服务器停止时退出循环，结束主程序

**CMakeLists.txt实现要求**
编写CMakeLists.txt文件，完成网关代码的构建
    - CMake最低版本3.10
    - 设置项目名称为GatewayService
    - 设置C++17标准
    - 添加可执行文件
    - 添加链接库

**其他要求**

1. 日志采用bitelog库，查看libSource/cpp-microservice-scaffold/source/log.h和log.cc文件
2. 服务发现实现，可参考libSource/cpp-microservice-scaffold/test/etcd_brpc/rpc_client.cc 和 chat2data-tech/example/etcd/gateway.cc

**输出要求**

1. 所有文件创建在chat2data-tech/chat2Data/svc_gatewayService目录下
   - gatewayServer.h
   - gatewayServer.cc
   - gatewayServiceImpl.h
   - gatewayServiceImpl.cc
   - main.cc
   - CMakeLists.txt
2. 所有代码必须符合项目规则，代码生成完成之后，必须检查是否符合项目规则文档中要求，务必确保符合项目规则约定
3. 无需生成测试代码

请你先严格按照上述要求，理解网关子服务的实现需求，然后给我复述下你的实现思路。在我确保你和我理解一致后，我再告诉你逐步实现。最后，在具体实现代码前，必须先详细阅读下项目规则约定，确保生成的代码符合项目规则。
