#pragma once

#include <memory>
#include <string>

namespace notifyService {

/**
 * @brief 邮件客户端配置
 */
struct MailSettings {
    std::string _username; // 邮箱用户名（即邮箱号）
    std::string _password; // 邮箱授权码
    std::string _url;      // 邮件服务器地址
    std::string _from;     // 发件人邮箱
};

/**
 * @brief 邮件发送抽象基类
 *
 * 使用 libcurl 封装发送邮件的主体流程与读取回调，
 * 邮件正文由具体子类通过 buildEmailBody 构造。
 */
class EmailSender {
public:
    using ptr = std::shared_ptr<EmailSender>;

    /**
     * @brief 构造邮件发送器
     * @param settings 邮箱配置
     */
    explicit EmailSender(const MailSettings& settings);

    virtual ~EmailSender() = default;

    /**
     * @brief 发送邮件
     * @param toEmail 收件人邮箱
     * @param subject 邮件主题
     * @param content 邮件内容（HTML格式）
     * @return 发送成功返回 true, 失败返回 false
     */
    bool sendEmail(const std::string& toEmail, const std::string& subject, const std::string& content);

protected:
    /**
     * @brief 构造邮件正文（包含主题头与HTML内容）
     * @param subject 邮件主题
     * @param content 邮件内容
     * @return 完整的邮件正文
     */
    virtual std::string buildEmailBody(const std::string& subject, const std::string& content) const = 0;

private:
    /**
     * @brief libcurl 读取回调
     * @param buffer   存放邮件内容的缓冲区
     * @param size     单个数据块大小
     * @param nitems   数据块个数
     * @param userdata 用户数据，这里指向邮件正文流
     * @return 实际读取的字节数
     */
    static size_t readCallback(char* buffer, size_t size, size_t nitems, void* userdata);

private:
    MailSettings _settings; // 邮箱配置
};

} // namespace notifyService
