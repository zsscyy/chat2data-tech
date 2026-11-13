#pragma once

#include <string>

#include "emailSender.h"

namespace notifyService {

/**
 * @brief 普通邮件发送器
 *
 * 负责构造普通邮件的正文，发送流程复用 EmailSender。
 */
class NormalEmailSender : public EmailSender {
public:
    /**
     * @brief 构造普通邮件发送器
     * @param settings 邮箱配置
     */
    explicit NormalEmailSender(const MailSettings& settings);

    ~NormalEmailSender() override = default;

protected:
    /**
     * @brief 构造普通邮件正文
     * @param subject 邮件主题
     * @param content 邮件内容（HTML格式）
     * @return 完整的邮件正文
     */
    std::string buildEmailBody(const std::string& subject, const std::string& content) const override;
};

} // namespace notifyService
