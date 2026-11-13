#pragma once

#include <string>

#include "emailSender.h"

namespace notifyService {

/**
 * @brief 邮件发送任务
 */
struct EmailTask {
    std::string _toEmail;     // 收件人邮箱
    std::string _subject;     // 邮件主题
    std::string _content;     // 邮件内容
    EmailSender::ptr _sender; // 邮件发送器
};

} // namespace notifyService
