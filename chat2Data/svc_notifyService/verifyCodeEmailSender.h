#pragma once

#include <string>

#include "emailSender.h"

namespace notifyService {

/**
 * @brief 验证码邮件发送器
 *
 * 负责构造验证码邮件的正文，发送流程复用 EmailSender。
 */
class VerifyCodeEmailSender : public EmailSender {
public:
    /**
     * @brief 构造验证码邮件发送器
     * @param settings 邮箱配置
     */
    explicit VerifyCodeEmailSender(const MailSettings& settings);

    ~VerifyCodeEmailSender() override = default;

protected:
    /**
     * @brief 构造验证码邮件正文
     * @param subject 邮件主题
     * @param content 验证码
     * @return 完整的邮件正文
     */
    std::string buildEmailBody(const std::string& subject, const std::string& content) const override;
};

} // namespace notifyService
