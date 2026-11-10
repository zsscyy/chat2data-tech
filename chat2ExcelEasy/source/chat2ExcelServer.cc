#include "chat2ExcelServer.h"
#include <httplib.h>
#include <log.h>
#include <util.h>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <vector>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include <ai_chat_sdk/common.h>

namespace chat2excel {

Chat2ExcelServer::Chat2ExcelServer(const std::string& host, int port)
    : _host(host)
    , _port(port)
    , _server(std::make_unique<httplib::Server>())
    , _isRunning(false)
    , _chatSDK(std::make_shared<ai_chat_sdk::ChatSDK>()) {
    INF("Chat2ExcelServer initialized with host: {}, port: {}", _host, _port);
    
    if (!initChatSDK()) {
        ERR("ChatSDK initialization failed!");
    }
}

Chat2ExcelServer::~Chat2ExcelServer() {
    if (_isRunning.load()) {
        stop();
    }
}

bool Chat2ExcelServer::start() {
    // 检查服务器是否已在运行
    if (_isRunning.load()) {
        ERR("Chat2ExcelServer is already running!");
        return false;
    }

    // 检查服务器实例是否有效
    if (!_server) {
        ERR("Chat2ExcelServer instance is null!");
        return false;
    }

    // 设置HTTP路由规则
    setRoutes();

    // 创建独立线程启动服务器
    // 使用lambda表达式捕获this指针，访问服务器实例
    std::thread serverThread([this]() {
        INF("Chat2ExcelServer starting on {}:{}", _host, _port);
        // listen()方法会阻塞，直到服务器停止
        if (!_server->listen(_host, _port)) {
            ERR("Chat2ExcelServer failed to listen on {}:{}", _host, _port);
        }
    });

    // 将线程与主线程分离，服务器在后台独立运行
    serverThread.detach();
    
    // 标记服务器状态为运行中
    _isRunning.store(true);
    INF("Chat2ExcelServer started successfully!");
    return true;
}

void Chat2ExcelServer::stop() {
    // 检查服务器是否正在运行
    if (!_isRunning.load()) {
        ERR("Chat2ExcelServer is not running!");
        return;
    }

    // 停止HTTP服务器
    if (_server) {
        _server->stop();
    }

    // 标记服务器状态为未运行
    _isRunning.store(false);
    INF("Chat2ExcelServer stopped successfully!");
}

bool Chat2ExcelServer::isRunning() const {
    return _isRunning.load();
}

void Chat2ExcelServer::setRoutes() {
    // 设置静态资源路径，将根路径映射到build/www目录
    // 这样访问http://host:port/就会自动返回index.html
    // 兼容不同的运行目录：项目根目录、build目录、source/build目录
    std::vector<std::string> staticCandidates = {
        "./build/www",
        "./www",
        "../../build/www",
        "../build/www",
        "./chat2ExcelEasy/build/www"
    };
    std::string staticDir = staticCandidates.front();
    for (const auto& candidate : staticCandidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
            staticDir = candidate;
            break;
        }
    }
    _server->set_mount_point("/", staticDir);
    INF("Static files mounted at {}", staticDir);

    // 注册健康检查路由
    // GET /health -> handleHealthCheck()
    _server->Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealthCheck(req, res);
    });

    // 注册文件上传路由
    // POST /api/uploadFile -> handleUploadFile()
    _server->Post("/api/uploadFile", [this](const httplib::Request& req, httplib::Response& res) {
        handleUploadFile(req, res);
    });

    // 注册发送消息路由
    // POST /api/sendMessage -> handleSendMessage()
    _server->Post("/api/sendMessage", [this](const httplib::Request& req, httplib::Response& res) {
        handleSendMessage(req, res);
    });

    INF("HTTP routes set successfully!");
}

void Chat2ExcelServer::handleHealthCheck(const httplib::Request& req, httplib::Response& res) {
    // 检查服务器运行状态
    if (_isRunning.load()) {
        // 服务器健康，返回成功响应
        std::string responseJson = R"({"status": "healthy"})";
        res.set_content(responseJson, "application/json");
        res.status = 200;  // HTTP 200 OK
        INF("Health check request handled - server is healthy");
    } else {
        // 服务器不健康，返回错误响应
        std::string responseJson = R"({"status": "unhealthy", "message": "Server is not running"})";
        res.set_content(responseJson, "application/json");
        res.status = 503;  // HTTP 503 Service Unavailable
        WRN("Health check request handled - server is unhealthy");
    }
}

void Chat2ExcelServer::sendErrorResponse(httplib::Response& res, const std::string& errorMessage, int statusCode) {
    Json::Value errorResponse;
    errorResponse["success"] = false;
    errorResponse["error"] = errorMessage;
    auto jsonStr = biteutil::JSON::serialize(errorResponse);
    res.set_content(jsonStr.value_or("{}"), "application/json");
    res.status = statusCode;
}

void Chat2ExcelServer::handleUploadFile(const httplib::Request& req, httplib::Response& res) {
    INF("Received file upload request");

    // 1. 从查询参数中获取文件名
    if (!req.has_param("fileName")) {
        ERR("Missing fileName parameter in request");
        sendErrorResponse(res, "Missing fileName parameter", 400);
        return;
    }

    std::string fileName = req.get_param_value("fileName");
    INF("Uploading file: {}", fileName);

    // 检查请求体是否为空
    if (req.body.empty()) {
        ERR("Empty file content in request body");
        sendErrorResponse(res, "Empty file content", 400);
        return;
    }

    // 2. 创建保存文件的目录（如果不存在）
    std::string excelFileDir = "excelFile";
    std::filesystem::path dirPath(excelFileDir);
    std::error_code ec;
    if (!std::filesystem::exists(dirPath)) {
        if (!std::filesystem::create_directory(dirPath, ec)) {
            ERR("Failed to create directory: {}, error: {}", excelFileDir, ec.message());
            sendErrorResponse(res, "Failed to create directory", 500);
            return;
        }
        INF("Directory created: {}", excelFileDir);
    }

    // 3. 生成文件ID（基于当前时间戳）
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    std::string fileId = std::to_string(timestamp);

    // 构建完整的文件保存路径
    std::string filePath = excelFileDir + "/" + fileName;

    // 4. 保存文件到本地
    if (!biteutil::FUTIL::write(filePath, req.body)) {
        ERR("Failed to save file: {}", filePath);
        sendErrorResponse(res, "Failed to save file", 500);
        return;
    }

    INF("File saved successfully: {}", filePath);

    // 5. 创建文件ID到路径的映射
    {
        std::lock_guard<std::mutex> lock(_fileMutex);
        _fileIdToPath[fileId] = filePath;
    }
    INF("File ID mapping created: {} -> {}", fileId, filePath);

    // 6. 构造响应结果并返回
    Json::Value response;
    response["success"] = true;
    response["result"]["fileId"] = fileId;

    auto jsonStr = biteutil::JSON::serialize(response);
    if (!jsonStr.has_value()) {
        ERR("Failed to serialize response JSON");
        sendErrorResponse(res, "JSON serialization failed", 500);
        return;
    }

    res.set_content(jsonStr.value(), "application/json");
    res.status = 200;
    INF("File upload completed successfully, fileId: {}", fileId);
}

std::string Chat2ExcelServer::parseExcelFile(const std::string& fileId) {
    INF("Parsing Excel file with fileId: {}", fileId);

    // 1. 通过fileId在文件id和路径映射中找到该fileId对应的路径
    std::string filePath;
    {
        std::lock_guard<std::mutex> lock(_fileMutex);
        auto it = _fileIdToPath.find(fileId);
        if (it == _fileIdToPath.end()) {
            ERR("File ID not found: {}", fileId);
            return "";
        }
        filePath = it->second;
    }
    INF("Found file path for fileId {}: {}", fileId, filePath);

    // 2. 检查文件是否存在
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        ERR("File does not exist: {}, error: {}", filePath, ec.message());
        return "";
    }

    // 3. 通过std::filesystem获取文件大小，预留空间
    auto fileSize = std::filesystem::file_size(filePath, ec);
    if (ec) {
        ERR("Failed to get file size: {}, error: {}", filePath, ec.message());
        return "";
    }
    INF("File size: {} bytes", fileSize);

    // 预留空间，避免频繁重新分配内存
    std::string result;
    result.reserve(fileSize * 2);

    // 4. 解析Excel文件内容
    // 4.1 通过ExcelParser获取Excel文件中的worksheet名列表
    std::vector<std::string> worksheetNames = ExcelParser::getWorksheetNames(filePath);
    if (worksheetNames.empty()) {
        ERR("No worksheets found in file: {}", filePath);
        return "";
    }
    INF("Found {} worksheets in file", worksheetNames.size());

    // 4.2 循环解析每个worksheet数据
    int worksheetIndex = 1;
    for (const auto& sheetName : worksheetNames) {
        // 添加worksheet标识头
        result += "worksheet" + std::to_string(worksheetIndex) + ": " + sheetName + "\n";

        // 通过ExcelParser获取指定worksheet数据
        std::string sheetData = ExcelParser::getWorksheetData(filePath, sheetName);
        if (!sheetData.empty()) {
            result += sheetData;
            // 如果数据不以换行符结尾，添加换行符
            if (!sheetData.empty() && sheetData.back() != '\n') {
                result += "\n";
            }
        } else {
            WRN("Empty data for worksheet: {}", sheetName);
        }

        // 多个worksheet之间用空行分隔（最后一个worksheet后不加空行）
        if (worksheetIndex < worksheetNames.size()) {
            result += "\n";
        }

        worksheetIndex++;
    }

    INF("Excel file parsed successfully, total size: {} bytes", result.size());
    return result;
}

std::string Chat2ExcelServer::buildPrompt(const std::string& filename, const std::string& excelData, const std::string& userQuestion) {
    INF("Building prompt with filename: {}, excelData size: {}, userQuestion: {}", 
        filename, excelData.size(), userQuestion);

    std::string promptTemplate = R"(## 角色定义
你是一个专业的Excel数据分析专家, 能够根据提供的Excel数据, 直接进行数据分析并给出具体的分析结果。

## 输出要求(必须严格遵守)
回答必须包含以下结构：

### 标题
用一句话(10-20字)提炼本次分析的核心结论或主题

### 分析结果
根据用户问题，直接展示数据分析得到的具体数值、统计结果、对比数据等。要求：
- 以表格或文本形式呈现主要数据结果
- 数据要具体、准确, 基于提供的Excel内容
- 如有排名, 给出具体排名列表
- 如有对比, 明确标注差异和特点

### 数据洞察
从分析结果中提炼3-5个关键发现或规律, 例如：
- 最高/最低表现的专业或班级
- 成绩分布的特点
- 异常或值得关注的数据点

### 总结与建议
- 综合概括本次分析的核心结论
- 基于数据提出1-2条具体可行的建议
    
## 重要约束
- 直接输出分析结果，以"已经为您完成分析，结果如下："开头
- 所有数据必须从提供的Excel内容中提取计算
- 回答必须使用中文

## 数据上下文
excel文件名:{filename}

excel文件内容:{excelData}

## 用户问题
用户输入:{userQuestion}
)";

    std::string result = promptTemplate;
    
    size_t pos;
    
    pos = result.find("{filename}");
    if (pos != std::string::npos) {
        result.replace(pos, 10, filename);
    }
    
    pos = result.find("{excelData}");
    if (pos != std::string::npos) {
        result.replace(pos, 11, excelData);
    }
    
    pos = result.find("{userQuestion}");
    if (pos != std::string::npos) {
        result.replace(pos, 14, userQuestion);
    }

    INF("Prompt built successfully, total size: {} bytes", result.size());
    return result;
}

bool Chat2ExcelServer::initChatSDK() {
    INF("Initializing ChatSDK...");
    auto deepseekConfig = std::make_shared<ai_chat_sdk::APIConfig>();
    deepseekConfig->_modelName = "deepseek-chat";
    deepseekConfig->_apiKey = std::getenv("deepseek_apikey");
    deepseekConfig->_temperature = 0.7;
    deepseekConfig->_maxTokens = 4096;

    std::vector<std::shared_ptr<ai_chat_sdk::Config>> modelConfigs = { deepseekConfig };

    // 配置GLM-4-Flash（API Key从环境变量读取，未设置则跳过）
    const char* glmApiKey = std::getenv("glm_apikey");
    if (glmApiKey != nullptr && glmApiKey[0] != '\0') {
        auto glmConfig = std::make_shared<ai_chat_sdk::APIConfig>();
        glmConfig->_modelName = "glm-4-flash";
        glmConfig->_apiKey = glmApiKey;
        glmConfig->_temperature = 0.7;
        glmConfig->_maxTokens = 4096;
        modelConfigs.push_back(glmConfig);
        INF("glm-4-flash config added");
    }

    // 配置MiniMax-M2.7-highspeed（API Key从环境变量读取，未设置则跳过）
    const char* minimaxApiKey = std::getenv("minimax_apikey");
    if (minimaxApiKey != nullptr && minimaxApiKey[0] != '\0') {
        auto minimaxConfig = std::make_shared<ai_chat_sdk::APIConfig>();
        minimaxConfig->_modelName = "MiniMax-M2.7-highspeed";
        minimaxConfig->_apiKey = minimaxApiKey;
        minimaxConfig->_temperature = 0.7;
        minimaxConfig->_maxTokens = 4096;
        modelConfigs.push_back(minimaxConfig);
        INF("MiniMax-M2.7-highspeed config added");
    }

    if (!_chatSDK->initModels(modelConfigs)) {
        ERR("ChatSDK initModels failed!");
        return false;
    }

    _sessionId = _chatSDK->createSession("deepseek-chat");
    if (_sessionId.empty()) {
        ERR("Failed to create session for deepseek-chat");
        return false;
    }

    INF("ChatSDK initialized successfully, sessionId: {}", _sessionId);
    return true;
}

std::string Chat2ExcelServer::getFileNameFromPath(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return filePath.substr(lastSlash + 1);
    }
    return filePath;
}

void Chat2ExcelServer::handleSendMessage(const httplib::Request& req, httplib::Response& res) {
    INF("Received sendMessage request");

    Json::Value requestJson;
    Json::Reader reader;
    if (!reader.parse(req.body, requestJson)) {
        ERR("Failed to parse request body as JSON");
        sendErrorResponse(res, "Invalid JSON format in request body", 400);
        return;
    }

    std::string fileId = requestJson.get("fileId", "").asString();
    std::string message = requestJson.get("message", "").asString();

    if (fileId.empty()) {
        ERR("fileId is empty");
        sendErrorResponse(res, "fileId is required", 400);
        return;
    }

    if (message.empty()) {
        ERR("message is empty");
        sendErrorResponse(res, "message is required", 400);
        return;
    }

    INF("Processing message for fileId: {}, message: {}", fileId, message);

    std::string filePath;
    {
        std::lock_guard<std::mutex> lock(_fileMutex);
        auto it = _fileIdToPath.find(fileId);
        if (it == _fileIdToPath.end()) {
            ERR("File ID not found: {}", fileId);
            sendErrorResponse(res, "File ID not found", 404);
            return;
        }
        filePath = it->second;
    }

    std::string excelData = parseExcelFile(fileId);
    if (excelData.empty()) {
        ERR("Failed to parse Excel file for fileId: {}", fileId);
        sendErrorResponse(res, "Failed to parse Excel file", 500);
        return;
    }

    std::string filename = getFileNameFromPath(filePath);
    std::string prompt = buildPrompt(filename, excelData, message);

    INF("Prompt built successfully, sending to model...");

    res.status = 200;
    res.set_header("Content-Type", "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Headers", "*");

    res.set_chunked_content_provider("text/event-stream", 
        [this, prompt](size_t offset, httplib::DataSink& dataSink) -> bool {
            auto writeChunk = [&](const std::string& chunk, bool last) {
                std::string sseData = "data: " + Json::valueToQuotedString(chunk.c_str()) + "\n\n";
                dataSink.write(sseData.c_str(), sseData.size());

                if (last) {
                    std::string doneData = "data: [DONE]\n\n";
                    dataSink.write(doneData.c_str(), doneData.size());
                    dataSink.done();
                    return false;
                }
                return true;
            };

            if (!writeChunk("", false)) {
                return false;
            }

            _chatSDK->sendMessageStream(_sessionId, prompt, writeChunk);

            return false;
        });

    INF("sendMessage request handled successfully");
}

}
