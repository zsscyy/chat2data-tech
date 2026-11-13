#include "verifyCodeEmailSender.h"

#include <sstream>
#include <string>

namespace notifyService {

VerifyCodeEmailSender::VerifyCodeEmailSender(const MailSettings& settings) : EmailSender(settings) {}

std::string VerifyCodeEmailSender::buildEmailBody(const std::string& subject, const std::string& content) const {
    std::ostringstream body;
    body << "Subject: " << subject << "\r\n";
    body << "Content-Type: text/html; charset=UTF-8\r\n";
    body << "\r\n";
    body << "<html><body><p>您的验证码：<b>" << content << "</b></p>"
         << "<p>验证码将在 5 分钟后失效，请勿泄露给他人。</p></body></html>\r\n";
    return body.str();
}

} // namespace notifyService
