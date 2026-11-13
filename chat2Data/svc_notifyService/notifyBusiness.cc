#include "notifyBusiness.h"

#include <memory>
#include <utility>

#include <curl/curl.h>

#include <bite_scaffold/log.h>

#include "errorHandler.h"
#include "normalEmailSender.h"
#include "verifyCodeEmailSender.h"

namespace notifyService {

namespace {

// 验证码邮件主题
const char* const VERIFY_CODE_SUBJECT = "验证码";

} // namespace

NotifyBusiness::NotifyBusiness(const MailSettings& mailSettings) {
    // 1. 初始化 curl 全局配置
    CURLcode ret = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (ret != CURLE_OK) {
        ERR("初始化CURL全局配置失败: {}", curl_easy_strerror(ret));
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }

    // 2. 创建验证码邮件与普通邮件发送器
    _verifyCodeEmailSender = std::make_shared<VerifyCodeEmailSender>(mailSettings);
    _normalEmailSender = std::make_shared<NormalEmailSender>(mailSettings);
    INF("NotifyBusiness initialized");
}

NotifyBusiness::~NotifyBusiness() {
    // 先停止工作线程，确保不会再有邮件发送动作使用 curl，再清理全局资源
    stop();
    curl_global_cleanup();
}

void NotifyBusiness::start() {
    _emailWorker.start();
    INF("NotifyBusiness started");
}

void NotifyBusiness::stop() {
    _emailWorker.stop();
    INF("NotifyBusiness stopped");
}

void NotifyBusiness::sendVerifyCodeEmail(const std::string& email, const std::string& code) {
    EmailTask task;
    task._toEmail = email;
    task._subject = VERIFY_CODE_SUBJECT;
    task._content = code;
    task._sender = _verifyCodeEmailSender;

    if (!_emailWorker.addTask(std::move(task))) {
        ERR("验证码邮件任务入队失败, email: {}", email);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }
    INF("验证码邮件任务已入队, email: {}", email);
}

void NotifyBusiness::sendNormalEmail(const std::string& toEmail, const std::string& subject,
                                     const std::string& content) {
    EmailTask task;
    task._toEmail = toEmail;
    task._subject = subject;
    task._content = content;
    task._sender = _normalEmailSender;

    if (!_emailWorker.addTask(std::move(task))) {
        ERR("普通邮件任务入队失败, toEmail: {}", toEmail);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }
    INF("普通邮件任务已入队, toEmail: {}", toEmail);
}

} // namespace notifyService
