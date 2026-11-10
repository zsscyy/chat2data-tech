#include <iostream>
#include <ai_chat_sdk/ChatSDK.h>
#include <ai_chat_sdk/util/myLog.h>

void sendMessageStream(ai_chat_sdk::ChatSDK& chatSDK, const std::string& sessionId)
{
    std::cout<<"--------------发送消息--------------"<<std::endl;
    std::cout << "user消息 > ";
    std::string message;
    std::getline(std::cin, message);
    std::cout << "--------------发送消息完成--------------" << std::endl;
    chatSDK.sendMessageStream(sessionId, message, [](const std::string& response, bool done){
        std::cout << "assistant消息: " << response << std::endl;
        if(done){
            std::cout << "--------------接收消息完成--------------" << std::endl;
        }
    });
}

int main(int argc, char* argv[])
{
    bite::Logger::initLogger("aiChatServer", "stdout", spdlog::level::info);

    ai_chat_sdk::ChatSDK chatSDK;
     // 初始化deepseek模型信息
    ai_chat_sdk::APIConfig deepseekConfig;
    deepseekConfig._apiKey = std::getenv("deepseek_apikey");  // 必须配置api_key
    deepseekConfig._temperature = 0.7;
    deepseekConfig._maxTokens = 2048;
    deepseekConfig._modelName = "deepseek-chat";

    std::vector<std::shared_ptr<ai_chat_sdk::Config>> configs;
    configs.push_back(std::make_shared<ai_chat_sdk::APIConfig>(deepseekConfig));

    // 初始化模型
    chatSDK.initModels(configs);

    std::cout << "--------------创建会话--------------" << std::endl;
    std::string sessionId = chatSDK.createSession("deepseek-chat");
    std::cout << "创建会话成功, 会话ID: " << sessionId << std::endl;

    int userOP = 1;
    while(true){
        std::cout<<"-------1. send message 0. exit-----------------"<<std::endl;
        std::cin>>userOP;
        if(userOP == 0){
            break;
        }
        getchar();
        sendMessageStream(chatSDK, sessionId);
    }

    std::cout << "--------------程序退出--------------" << std::endl;
    return 0;
}