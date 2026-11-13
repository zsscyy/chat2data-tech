#include "emailSender.h"

#include <curl/curl.h>

#include <memory>
#include <sstream>
#include <string>

#include <bite_scaffold/log.h>

namespace notifyService {

namespace {

// 连接超时时间（秒）
const long CONNECT_TIMEOUT_SECONDS = 15L;
// 请求总超时时间（秒）
const long REQUEST_TIMEOUT_SECONDS = 30L;

/**
 * @brief curl 句柄删除器，保证句柄自动释放
 */
struct CurlHandleDeleter {
    void operator()(CURL* handle) const {
        if (handle != nullptr) {
            curl_easy_cleanup(handle);
        }
    }
};

/**
 * @brief curl 收件人链表删除器，保证链表自动释放
 */
struct CurlSlistDeleter {
    void operator()(curl_slist* list) const {
        if (list != nullptr) {
            curl_slist_free_all(list);
        }
    }
};

using CurlHandlePtr = std::unique_ptr<CURL, CurlHandleDeleter>;
using CurlSlistPtr = std::unique_ptr<curl_slist, CurlSlistDeleter>;

/**
 * @brief 设置 libcurl 选项，失败时记录日志
 * @param handle     libcurl 句柄
 * @param option     选项
 * @param value      选项值
 * @param optionName 选项名称，用于日志
 * @return 设置成功返回 true
 */
template <typename T>
bool applyOption(CURL* handle, CURLoption option, T value, const char* optionName) {
    CURLcode ret = curl_easy_setopt(handle, option, value);
    if (ret != CURLE_OK) {
        ERR("设置CURL参数失败, option: {}, 原因: {}", optionName, curl_easy_strerror(ret));
        return false;
    }
    return true;
}

} // namespace

EmailSender::EmailSender(const MailSettings& settings) : _settings(settings) {}

bool EmailSender::sendEmail(const std::string& toEmail, const std::string& subject, const std::string& content) {
    // 1. 创建 curl 操作句柄
    CurlHandlePtr curl(curl_easy_init());
    if (!curl) {
        ERR("构造CURL操作句柄失败");
        return false;
    }

    // 2. 设置超时、SSL、服务器地址以及账号信息
    // 不校验对端证书，仅适用于开发测试阶段
    if (!applyOption(curl.get(), CURLOPT_CONNECTTIMEOUT, CONNECT_TIMEOUT_SECONDS, "CURLOPT_CONNECTTIMEOUT") ||
        !applyOption(curl.get(), CURLOPT_TIMEOUT, REQUEST_TIMEOUT_SECONDS, "CURLOPT_TIMEOUT") ||
        !applyOption(curl.get(), CURLOPT_SSL_VERIFYPEER, 0L, "CURLOPT_SSL_VERIFYPEER") ||
        !applyOption(curl.get(), CURLOPT_SSL_VERIFYHOST, 0L, "CURLOPT_SSL_VERIFYHOST") ||
        !applyOption(curl.get(), CURLOPT_URL, _settings._url.c_str(), "CURLOPT_URL") ||
        !applyOption(curl.get(), CURLOPT_USERNAME, _settings._username.c_str(), "CURLOPT_USERNAME") ||
        !applyOption(curl.get(), CURLOPT_PASSWORD, _settings._password.c_str(), "CURLOPT_PASSWORD") ||
        !applyOption(curl.get(), CURLOPT_MAIL_FROM, _settings._from.c_str(), "CURLOPT_MAIL_FROM")) {
        return false;
    }

    // 3. 设置收件人列表
    CurlSlistPtr recipients(curl_slist_append(nullptr, toEmail.c_str()));
    if (!recipients) {
        ERR("构造收件人列表失败, toEmail: {}", toEmail);
        return false;
    }
    if (!applyOption(curl.get(), CURLOPT_MAIL_RCPT, recipients.get(), "CURLOPT_MAIL_RCPT")) {
        return false;
    }

    // 4. 构造邮件正文并设置读取数据源与回调
    std::stringstream body(buildEmailBody(subject, content));
    if (body.str().empty()) {
        ERR("构造邮件正文失败, toEmail: {}", toEmail);
        return false;
    }
    if (!applyOption(curl.get(), CURLOPT_READDATA, static_cast<void*>(&body), "CURLOPT_READDATA") ||
        !applyOption(curl.get(), CURLOPT_READFUNCTION, &EmailSender::readCallback, "CURLOPT_READFUNCTION") ||
        !applyOption(curl.get(), CURLOPT_UPLOAD, 1L, "CURLOPT_UPLOAD")) {
        return false;
    }

    // 5. 执行邮件发送请求
    CURLcode ret = curl_easy_perform(curl.get());
    if (ret != CURLE_OK) {
        ERR("请求邮件服务器失败, toEmail: {}, 原因: {}", toEmail, curl_easy_strerror(ret));
        return false;
    }

    INF("邮件发送成功, toEmail: {}, subject: {}", toEmail, subject);
    return true;
}

size_t EmailSender::readCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* bodyStream = static_cast<std::istream*>(userdata);
    bodyStream->read(buffer, static_cast<std::streamsize>(size * nitems));
    return static_cast<size_t>(bodyStream->gcount());
}

} // namespace notifyService
