
## Chat2Data网关服务接口设计

客户端与网关之间通过HTTP协议通信，通信接口基本都是基于restful风格接口设计，总共30个接口，29个业务接口以及健康检测接口。

### 1. 健康检测

定期检测服务器是否正常运行。
**请求URL：GET /health**
**响应参数**

```json
{
    "status": "healthy",
    "service": "GatewayService",
    "timestamp": 1706428800
}
```

### 2. 用户子服务

用户子服务总共涉及到9个业务接口，具体定义如下。

#### 2.1 检测用户昵称是否唯一

**请求URL：POST /api/user/valid/nickname**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "nickname": "string"        // 用户昵称
}
```

**响应参数**

```json
{
    "requestId": "req_123456",    // 请求Id
    "errorCode": 0,               // 错误码
    "errorMsg": ""                // 错误描述
}
```

**错误码说明**

- 0: 成功
- 400: 请求参数错误
- 500: 服务内部错误
- 503: 后端服务不可用
  注意：请求成功时，错误描述errorMsg中不包含任何内容。

#### 2.2 检测用户邮箱是否唯一

**请求URL：POST /api/user/valid/email**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "email": "string"           // 用户邮箱
}
```

**响应参数**

```json
{
    "requestId": "req_123456",    // 请求Id
    "errorCode": 0,               // 错误码
    "errorMsg": ""                // 错误描述
}
```

#### 2.3 用户注册

**请求URL：POST /api/user/register**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "nickname": "string",       // 用户昵称
    "password": "string",       // 用户密码
    "email": "string"           // 用户邮箱
}
```

**响应参数**

```json
{
    "requestId": "req_123456",    // 请求Id
    "errorCode": 0,               // 错误码
    "errorMsg": ""                // 错误描述
}
```

#### 2.4 密码登录

**请求URL：POST /api/user/passwd/login**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "username": "string",       // 用户名昵称或邮箱
    "password": "string"        // 用户密码
}
```

**响应参数**

```json
{
    "requestId": "req_123456",    // 请求Id
    "errorCode": 0,               // 错误码
    "errorMsg": "",               // 错误描述
    "result": {                   // 返回结果
        "sessionId": "session_abc123"  // 会话Id
    }
}
```

#### 2.5 获取验证码

**请求URL：POST /api/user/code**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "email": "string"           // 用户邮箱
}
```

**响应参数**

```json
{
    "requestId": "req_123456",    // 请求Id
    "errorCode": 0,               // 错误码
    "errorMsg": "",               // 错误描述
    "result": {                   // 返回结果
        "codeId": "code_xyz789"   // 验证码Id
    }
}
```

验证码发送到用户邮箱。在验证码登录时，需要传递验证码Id 和 验证码，两者在后端匹配时才能登录成功。

#### 2.6 验证码登录

**请求URL：POST /api/user/vcode/login**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "email": "string",          // 用户邮箱
    "verifyCode": "string",     // 验证码
    "codeId": "string"          // 验证码ID
}
```

**响应参数**

```json
{
    "requestId": "req_123456",           // 请求ID
    "errorCode": 0,                      // 错误码
    "errorMsg": "",                      // 错误描述
    "result": {                          // 返回结果
        "sessionId": "session_abc123"    // 会话ID
    }
}
```

#### 2.7 会话登录

**请求URL：POST /api/user/session/login**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "sessionId": "string"       // 会话ID
}
```

**响应参数**

```json
{
    "requestId": "req_123456",    // 请求Id
    "errorCode": 0,               // 错误码
    "errorMsg": ""                // 错误描述
}
```

#### 2.8 退出登录

**请求URL：POST /api/user/logout**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "sessionId": "string"       // 会话ID
}
```

**响应参数**

```json
{
    "requestId": "req_123456",    // 请求ID
    "errorCode": 0,               // 错误码
    "errorMsg": ""                // 错误描述
}
```

#### 2.9 获取用户信息

**请求URL：POST /api/user/info?requestId={requestId}&sessionId={sessionId}&userId={userId}**
**响应参数**

```json
{
    "requestId": "req_123456",                 // 请求ID
    "errorCode": 0,                            // 错误码
    "errorMsg": "",                            // 错误描述
    "result": {                                // 返回结果Json
        "userInfo": {                          // 用户信息Json
            "userId": "user_001",              // 用户ID
            "nickname": "张三",                // 用户昵称
            "email": "zhangsan@example.com"   // 用户邮箱
        }
    }
}
```

### 3. 文件子服务

#### 3.1 上传文件信息

**请求URL：POST /api/file/upload/info**
**请求参数**

```json
{
    "requestId": "string",           // 请求ID
    "sessionId": "string",           // 会话ID
    "fileInfo": {
        "filename": "string",          // 文件名
        "fileSize": 0,                 // 文件大小
        "fileExt": "string"            // 文件扩展名，本项目仅支持.xlsx后缀的Excel文件
    },
    "chatSessionId": "string"        // 可选，关联的聊天会话ID
}
```

在上传文件时，不用提供聊天会话ID，当文件上传成功后，前端需要主动向后端发送关联文件和聊天会话的映射。

**响应参数**

```json
{
    "requestId": "req_123456",      // 请求ID
    "errorCode": 0,                 // 错误码
    "errorMsg": "",                 // 错误描述
    "result": {                     // 返回结果Json
        "fileId": "file_abc123"     // 文件ID
    }
}
```

#### 3.2 获取文件信息

**请求URL：GET /api/file/info?requestId={requestId}&sessionId={sessionId}&fileId={fileId}**
**响应参数**

```json
{
    "requestId": "req_123456",        // 请求ID
    "errorCode": 0,                   // 错误码
    "errorMsg": "",                   // 错误描述
    "result": {                       // 返回结果Json
        "fileId": "file_abc123",      // 文件ID
        "fileName": "员工薪资.xlsx",   // 文件名称
        "fileSize": 102400,           // 文件大小
        "uploadTime": 1706428800,     // 文件上传时间
        "fileExt": "xlsx",            // 文件后缀
    }
}
```

#### 3.3 上传文件数据

**请求URL：POST /api/file/upload?requestId={requestId}&sessionId={sessionId}&fileId={fileId}**
**请求头**：Content-Type: application/octet-stream
**请求体**：文件二进制数据
**响应结果**

```json
{
    "requestId": "req_123456",      // 请求ID
    "errorCode": 0,                 // 错误码
    "errorMsg": "",                 // 错误描述
    "result": {                     // 返回结果Json
        "fileId": "file_abc123"     // 文件ID
    }
}
```

#### 3.4 下载文件

**请求URL：GET /api/file/download?requestId={requestId}&sessionId={sessionId}&fileId={fileId}**
**响应头**：
Content-Type: application/octet-stream
Content-Disposition: attachment; filename="文件名"
**响应体**：文件二进制流

#### 3.5 删除文件

**请求URL：DELETE /api/file/{fileId}?requestId={requestId}&sessionId={sessionId}**
fileID为路径参数
**响应参数**

```json
{
    "requestId": "req_123456",    // 请求ID
    "errorCode": 0,               // 错误码
    "errorMsg": "删除成功"         // 错误描述
}
```

#### 3.6 预览Excel文件

**请求URL：POST /api/file/preview**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "sessionId": "string",      // 会话ID
    "fileId": "string",         // 文件ID
    "pageNumber": 1,            // 页码（可选，默认1）
    "pageSize": 50             // 每页行数（可选，默认50）
}
```

**响应结果**

```json
{
    "requestId": "req_123456",                       // 请求ID
    "errorCode": 0,                                  // 错误码
    "errorMsg": "",                                  // 错误描述
    "result": {                                      // 返回Json结果
        "fileId": "file_abc123",                     // 文件Id
        "fileName": "员工薪资.xlsx",                  // 文件名称
        "fileSize": 102400,                          // 文件大小
        "fileExt": "xlsx",                           // 文件后缀
        "excelData": {                               // excel内容
            "sheets": [                              // worksheet信息(数组)
                {                                    // worksheet Json
                    "name": "Sheet1",                // worksheet名称
                    "totalRows": 38,                 // 总行数
                    "colCount": 13,                 // 总列数
                    "currentPage": 1,                // 当前页码
                    "totalPages": 1,                 // 总页数
                    "pageSize": 50,                  // 每页行数
                    "columns": ["id", "姓名", "电话", "年龄", "部门", ...],  // 列名列表
                    "data": [                        // worksheet行数据
                        ["1", "曹玉凤", "15112345678", "22", "行政部", ...]  // 具体行数据
                    ]
                }
            ]
        }
    }
}
```

#### 3.7 获取用户文件列表

**请求URL：POST /api/file/list**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "sessionId": "string"       // 会话ID
}
```

**响应结果**

```json
{
    "requestId": "req_123456",                        // 请求ID
    "errorCode": 0,                                   // 错误码
    "errorMsg": "",                                   // 错误描述
    "result": {                                       // 返回结果Json
        "fileList": [                                 // 文件列表(数组)
            {                                         // 文件1Json对象
                "fileId": "file_001",                 // 文件ID
                "fileName": "员工薪资.xlsx",           // 文件名
                "fileSize": 102400,                   // 文件大小
                "uploadTime": 1706428800              // 文件上传时间
            },{                                       // 文件2JSon对象
                "fileId": "file_002",                 // 文件ID
                "fileName": "学生信息.xlsx",           // 文件名
                "fileSize": 204800,                   // 文件大小
                "uploadTime": 1706515200              // 文件上传时间
            }
       ]
    }
}
```

#### 3.8 关联文件和聊天会话映射

**请求URL：POST /api/file/chat/map**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "sessionId": "string",      // 用户会话ID
    "fileId": "string",         // 文件ID
    "chatSessionId": "string"   // 聊天会话ID
}
```

**响应结果**

```json
{
    "requestId": "req_123456",     // 请求ID
    "errorCode": 0,                // 错误码
    "errorMsg": ""                 // 错误描述
}
```

#### 3.9 上传SQLite文件

**请求URL：POST /api/file/sqlite/upload?requestId={requestId}&sessionId={sessionId}&filename={filename}**
**请求头**：Content-Type: application/octet-stream
**请求体**: SQLite 文件二进制数据
**响应结果**

```json
{
    "requestId": "req_123456",          // 请求ID
    "errorCode": 0,                     // 错误码
    "errorMsg": "",                     // 错误描述
    "result": {                         // 返回结果JSon
        "fileId": "file_sqlite_001"     // 文件ID
    }
}
```

### 4. 数据库子服务

数据库子服务总共涉及5个接口，具体定义如下。

#### 4.1 新建数据库连接

**请求URL：POST /api/db/connect**
**请求参数**

```json
{
    "requestId": "string",          // 请求ID
    "sessionId": "string",          // 会话ID
    "database": {
        "type": "MySQL",           // 数据库类型：MySQL 或 SQLite
        "MySQL": {                 // 当 type 为 MySQL 时填写
            "host": "string",      // MySQL 主机地址
            "port": 3306,          // MySQL 端口
            "name": "string",      // 数据库名称
            "username": "string",  // 用户名
            "password": "string",  // 密码
            "charset": "utf8mb4"   // 字符集
        },
        "SQLite": {                // 当 type 为 SQLite 时填写
            "fileId": "string",    // SQLite 文件ID
            "readonly": false      // 是否只读，默认 false
        }
    }
}
```

**响应结果**

```json
{
    "requestId": "req_123456",       // 请求ID
    "errorCode": 0,                  // 错误码
    "errorMsg": "",                  // 错误描述
    "result": {                      // 返回结果JSON
        "connectionId": "conn_001"   // 数据库连接ID
    }
}
```

注意：

- 程序启动时，会建立一个默认MySQL连接，连接ID为"excel_default"，专门处理智能Excel场景
- 智能DB场景，需要用户自己提交数据库信息建立数据库连接
- 智能DB场景，目前仅支持MySQL 和 SQLite

#### 4.2 断开数据库连接

**请求URL：POST /api/db/disconnect**
**请求参数**

```json
{
    "requestId": "string",      // 请求ID
    "sessionId": "string",      // 会话ID
    "connectionId": "string"    // 连接ID
}
```

**响应结果**

```json
{
    "requestId": "req_123456",    // 请求ID
    "errorCode": 0,               // 错误码
    "errorMsg": ""                // 错误描述
}
```

#### 4.3 获取数据库表列表

**请求URL：GET /api/db/tables?requestId={requestId}&sessionId={sessionId}&dbConnectId={dbConnectId}**
**响应结果**

```json
{
    "requestId": "req_123456",               // 请求ID
    "errorCode": 0,                          // 错误码
    "errorMsg": "",                          // 错误描述
    "result": {                              // 返回结果Json
        "tables": [                          // 返回数据库表列表数组
            "users", "orders", "products"    // 表列表信息
        ]
    }
}
```

#### 4.4 获取表数据

**请求URL：POST /api/db/table/data**
**请求参数**

```json
{
    "requestId": "string",       // 请求ID
    "sessionId": "string",       // 会话ID
    "dbConnectId": "string",     // 数据库连接ID
    "tableName": "string",       // 表名
    "forceOriginal": false       // 是否强制获取原始表数据（默认 false）
}
```

**响应结果**

```json
{
    "requestId": "req_123456",                     // 请求ID
    "errorCode": 0,                                // 错误码
    "errorMsg": "",                                // 错误描述
    "result": {                                    // 返回结果Json
        "tableSchema": {                           // 表结构
            "columnInfo": [                        // 列信息数组
                {                                  // 列1Json
                    "name": "id",                  // 列1名称
                    "type": "INT"                  // 列1类型
                },
                {                                  // 列2Json
                    "name": "name",                // 列2名称
                    "type": "VARCHAR"              // 类2类型
                }
            ],
            "tableData": {                         // 表数据
                "rows": [                          // 行数据数组
                    {
                        "cells": ["1", "张三"]     // 行1数据
                    },
                    {
                        "cells": ["2", "李四"]    // 行2数据
                    }
                ]
            }
        }
    }
}
```

#### 4.5 获取指定连接的临时表列表

**请求URL：POST /api/db/connection/status**
**请求参数**

```json
{
    "requestId": "string",       // 请求ID
    "sessionId": "string",       // 会话ID
    "dbConnectId": "string"      // 数据库连接ID
}
```

**响应结果**

```json
{
    "requestId": "req_123456",         // 请求ID
    "errorCode": 0,                    // 错误码
    "errorMsg": "",                    // 错误描述
    "result": {                        // 返回结果Json
        "tempTables": [                // 修改对表对应的新表名
            "users_temp", 
            "orders_temp"
        ],
        "hasModifications": true       // 是否为修改类SQL
    }
}
```

注意：为保证数据安全，修改类SQL没有再原表中操作，而是将原表复制一份新表，在新表上进行的修改操作。

### 5. AI子服务

AI子服务总共涉及6个结构，具体定义如下。

#### 5.1 获取支持模型列表

**请求URL：POST /api/ai/models**
**请求参数**

```json
{
    "requestId": "string",      // 请求Id
    "sessionId": "string"       // 会话Id
}
```

**响应结果**

```json
{
    "requestId": "string",                      // 请求ID
    "errorCode": 0,                             // 错误码
    "errorMsg": "",                             // 错误描述
    "result": {                                 // 返回结果Json
        "modelList": [                          // 模型列表数组
            {                                   // 模型1Json
                "modelName": "deepseek",        // 模型1名称
                "modelDesc": "DeepSeek 模型"    // 模型1描述
            },
            {                                  // 模型2Json                                
                "modelName": "gpt-4",          // 模型2名称
                "modelDesc": "GPT-4 模型"      // 模型2描述
            }
        ]
    }
}
```

#### 5.2 新建聊天会话

**请求URL：POST /api/ai/session/create**
**请求参数**

```json
{
    "requestId": "string",      // 请求Id
    "sessionId": "string",      // 会话ID
    "modelName": "string",      // 模型名称
    "title": "string"           // 会话标题
}
```

**响应结果**

```json
{
    "requestId": "string",                    // 请求ID
    "errorCode": 0,                           // 错误码
    "errorMsg": "",                           // 错误描述
    "result": {                               // 返回结果Json
        "chatSessionId": "chat_session_001",  // 聊天会话ID
        "modelName": "deepseek"               // 模型名称
    }
}
```

注意：

- 会话ID：用来区分当前登录用户
- 聊天会话ID：用来区分和模型聊天时不同会话
- 会话标题：新建会话时，请求中会话标题不用填写，等后续和模型聊天之后，用首条信息更新会话标题

#### 5.3 获取聊天会话列表

**请求URL：POST /api/ai/chatSessionLists**
**请求参数**

```json
{
    "requestId": "string",      // 请求Id
    "sessionId": "string"       // 聊天会话ID
}
```

**响应结果**

```json
{
    "requestId": "string",                                    // 请求ID
    "errorCode": 0,                                           // 错误码
    "errorMsg": "",                                           // 错误描述
    "result": {                                               // 返回结果Json
        "chatSessionLists": [                                 // 会话列表数组
            {                                                 // 会话1Json
                "chatSessionId": "chat_session_001",          // 会话1聊天会话ID
                "modelName": "deepseek",                      // 会话1模型名称
                "title": "数据分析",                           // 会话1标题
                "createdAt": 1706428800,                      // 会话1创建时间
                "updatedAt": 1706515200,                      // 会话1最近更新时间
                "messageCount": 10,                           // 会话1中总消息数量
                "firstUserMessageContent": "帮我分析一下这个表格" // 会话1首条消息内容
            },
            {                                                // 会话2Json
                "chatSessionId": "chat_session_002",         // 会话2聊天会话ID
                "modelName": "gpt-4",                        // 会话2模型名称
                "title": "SQL 查询",                          // 会话2标题
                "createdAt": 1706342400,                     // 会话2创建时间
                "updatedAt": 1706428800,                     // 会话2最近更新时间
                "messageCount": 5,                           // 会话2中总消息数量
                "firstUserMessageContent": "如何查询用户表"    // 会话2首条消息内容
            }
        ]
    }
}
```

#### 5.4 获取指定用户指定聊天会话历史消息

**请求URL：POST /api/ai/history**
**请求参数**

```json
{
    "requestId": "string",       // 请求Id
    "sessionId": "string",       // 会话ID
    "chatSessionId": "string"    // 聊天会话ID
}
```

**响应结果**

```json
{
    "requestId": "string",                           // 请求ID
    "errorCode": 0,                                  // 错误码
    "errorMsg": "",                                  // 错误描述
    "result": {                                      // 返回结果Json
        "messageList": [                             // 消息列表数组
            {                                        // 消息1Json对象
                "id": "msg_001",                     // 消息ID
                "role": "user",                      // 角色：user 或 assistant
                "content": "帮我分析一下这个表格",     // 消息内容
                "timestamp": 1706428800              // 消息创建时间
            },
            {                                        // 消息2Json对象
                "id": "msg_002",                     // 消息ID
                "role": "assistant",                 // 角色：user 或 assistant
                "content": "好的，我来帮您分析...",    // 消息内容
                "timestamp": 1706428805              // 消息创建时间
            }
        ],
        "fileId": "file_abc123",                         // 文件ID
        "sessionType": "excel",                         // 会话类型：excel/database
        "dbConnectionInfo": "db_connection_info",     // 数据库连接信息JSON（数据库场景）
    }
}
```

#### 5.5 删除指定用户指定聊天会话

**请求URL：POST /api/ai/delete**
**请求参数**

```json
{
    "requestId": "string",       // 请求Id
    "sessionId": "string",       // 会话ID
    "chatSessionId": "string"    // 聊天会话ID
}
```

**响应参数**

```json
{
    "requestId": "string",      // 请求ID
    "errorCode": 0,             // 错误码
    "errorMsg": ""              // 错误描述
}
```

#### 5.6 发送消息(流式)

**请求URL：POST /api/ai/sendStreamMessage**
**请求参数**

```json
{
    "requestId": "string",       // 请求Id
    "sessionId": "string",       // 会话ID
    "chatSessionId": "string",   // 聊天会话ID
    "chatType": "excel",         // 会话类型：plain/excel/database
    "message": "string",         // 用户消息内容
    "fileId": "string",          // 可选，文件ID(Excel场景)，数据库场景时该字段为空
    "dbConnectId": "string",     // 可选，数据库连接ID(数据库场景)，Excel场景时该字段为空
    "dbType": "DataType",        // 可选，数据库类型，数据库场景下，根据该字段确认具体操作哪个数据库
    "tableName": "string"        // 可选，数据库表名，数据库场景下，多个表名用逗号分隔
}
```

**响应格式**
Server-Sent Events (SSE) 流式响应
**响应数据格式**
data: {"content": "消息片段", "done": false, "errorCode": 0, "errorMsg": ""}
data: {"content": "更多消息", "done": false, "errorCode": 0, "errorMsg": ""}
data: [DONE]
**响应字段说明**

- content: 消息片段（字符串）
- done: 是否完成（布尔值）
- errorCode: 错误码（0 表示成功）
- errorMsg: 错误信息
