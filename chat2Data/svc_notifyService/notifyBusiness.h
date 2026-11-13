#pragma once

#include <memory>
#include <string>

#include "emailSender.h"
#include "emailWorker.h"

namespace notifyService {

/**
 * @brief 通知业务类
 *
 * 封装邮件发送器与异步工作线程，负责 curl 全局初始化与资源清理，
 * 并把发送验证码/普通邮件的请求转换为邮件任务入队。
 */
class NotifyBusiness {
public:
    using ptr = std::shared_ptr<NotifyBusiness>;

    /**
     * @brief 构造通知业务类
     * @param mailSettings 邮箱配置
     * @throws chat2Data::Chat2DataException curl 全局初始化失败
     */
    explicit NotifyBusiness(const MailSettings& mailSettings);

    ~NotifyBusiness();

    NotifyBusiness(const NotifyBusiness&) = delete;
    NotifyBusiness& operator=(const NotifyBusiness&) = delete;

    /**
     * @brief 启动邮件发送线程
     */
    void start();

    /**
     * @brief 停止邮件发送线程（排空队列后退出）
     */
    void stop();

    /**
     * @brief 发送验证码邮件
     * @param email 收件人邮箱
     * @param code  验证码
     * @throws chat2Data::Chat2DataException 任务入队失败
     */
    void sendVerifyCodeEmail(const std::string& email, const std::string& code);

    /**
     * @brief 发送普通邮件
     * @param toEmail 收件人邮箱
     * @param subject 邮件主题
     * @param content 邮件内容（HTML格式）
     * @throws chat2Data::Chat2DataException 任务入队失败
     */
    void sendNormalEmail(const std::string& toEmail, const std::string& subject, const std::string& content);

private:
    EmailSender::ptr _verifyCodeEmailSender; // 验证码邮件发送器
    EmailSender::ptr _normalEmailSender;     // 普通邮件发送器
    EmailWorker _emailWorker;                // 异步发送工作类
};

} // namespace notifyService
