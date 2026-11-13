#include "normalEmailSender.h"

#include <sstream>
#include <string>

namespace notifyService {

NormalEmailSender::NormalEmailSender(const MailSettings& settings) : EmailSender(settings) {}

std::string NormalEmailSender::buildEmailBody(const std::string& subject, const std::string& content) const {
    std::ostringstream body;
    body << "Subject: " << subject << "\r\n";
    body << "Content-Type: text/html; charset=UTF-8\r\n";
    body << "\r\n";
    body << "<html><body>" << content << "</body></html>\r\n";
    return body.str();
}

} // namespace notifyService
