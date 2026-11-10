**系统身份**：你是一个资深的C++后端开发工程师，在项目开发时经常使用自定义异常处理项目中异常以及错误情况

**任务**：帮我为chat2Data项目实现错误码、错误码枚举类ErrorCode定义以及Chat2DataException异常类定义

**错误码说明**
- 错误码放在ErrorCode枚举类中，ErrorCode枚举使用C++11标准定义，枚举值为整数类型
- SUCCESS为通用成功码，枚举值是0
- 用户子服务错误码范围100~199，错误码格式：USER_xxx_xxx,比如：USER_NICKNAME_EXISTS 用户昵称已存在
- 文件子服务错误码范围200~299，错误码格式：FILE_xxx_xxx,比如：FILE_NOT_FOUND 文件不存在
- 数据库子服务错误码范围300~399，错误码格式：DB_xxx_xxx,比如：DB_CONNECTION_FAILED 数据库连接失败
- Excel解析子服务错误码范围400~499，错误码格式：EXCEL_xxx_xxx,比如：EXCEL_PARSE_FAILED Excel解析失败
- 通知子服务错误码范围500~599，错误码格式：NOTIFY_xxx_xxx,比如：NOTIFY_SEND_FAILED 通知发送失败
- AI子服务错误码范围600~699，错误码格式：AI_xxx_xxx,比如：AI_MODEL_NOT_FOUND AI模型不存在
- 网关子服务错误码范围700~799，错误码格式：GATEWAY_xxx_xxx,比如：GATEWAY_CONNECTION_FAILED 网关连接失败
注意：
- 目前，在ErrorCode枚举类中只定义一个通用成功码SUCCESS，枚举值是0
- 其余子服务的错误码，将在后续项目实现时根据实际情况进行添加，但每个子服务错误码范围注释需要添加上
- 添加全局方法error2String，接收一个错误码，返回错误码对应的错误描述符，要求：
    1. 实现时采用unorder_map建立错误码和描述符映射
    2. 映射关系举例如下：
        {ErrorCode::SUCCESS, "操作成功"},
        {ErrorCode::USER_NICKNAME_EXISTS, "用户昵称已存在"},
    3. 每个子服务具体的错误码描述符后续在添加，需要添加各个子服务错误码描述符注释区分
    4. 如果错误码存在返回对应的错误描述符，否则返回"未知错误"
- 添加全局方法getServiceName，接收一个错误码，返回错误码所属的子服务名称，要求：
    1. 网关子服务名称：gatewayService
    2. 用户子服务名称：userService
    3. 文件子服务名称：fileService
    4. 数据库子服务名称：dbService
    5. Excel解析子服务名称：excelService
    6. 通知子服务名称：notifyService
    7. AI子服务名称：aiService
    8. 验证码不存在时，返回"未知服务"

**Chat2DataException说明**
1. Chat2DataException异常类继承自std::exception类，用于处理chat2Data项目中发生的异常情况
2. Chat2DataException异常类中，添加构造函数，接收一个错误码
3. Chat2DataException异常类中，重写what()方法，返回错误描述符的指针，格式要求："[服务名称]: 错误描述符"
4. Chat2DataException异常类中，添加一个成员变量，用于存储错误码和错误描述符，要求：
    - 错误码：ErrorCode枚举类中的值
    - 错误描述符：error2String方法返回的错误描述符字符串

**输出要求**
1. 头文件：errorHandler.h
    - 存放枚举类ErrorCode定义
    - 存放全局方法error2String、getServiceName的声明
    - 存放Chat2DataException异常类的声明
2. 源文件：errorHandler.cc
    - 实现全局方法error2String、getServiceName的实现代码
    - 实现Chat2DataException异常类的实现代码
3. 代码文件放置在chat2data-tech/chat2Data/common目录下

请你严格按照上述要求帮我实现错误码、枚举类型以及异常类的声明和定义。
