#pragma once

#include <memory>

#include <brpc/server.h>

#include "errorHandler.h"
#include "notifyService.pb.h"

namespace notifyService {

class NotifyBusiness;

/**
 * @brief 通知子服务 RPC 接口实现
 *
 * 实现发送验证码邮件与发送普通邮件两个 RPC 接口，
 * 只负责参数校验与响应填充，不负责 RPC 服务器搭建。
 */
class NotifyServiceImpl : public ::chat2Data::notifyService::NotifyService {
public:
    using ptr = std::shared_ptr<NotifyServiceImpl>;

    /**
     * @brief 构造 RPC 接口实现对象
     * @param notifyBusiness 通知业务类
     */
    explicit NotifyServiceImpl(std::shared_ptr<NotifyBusiness> notifyBusiness);

    ~NotifyServiceImpl() override = default;

    void SendVerifyCode(::google::protobuf::RpcController* controller,
                        const ::chat2Data::notifyService::SendVerifyCodeRequest* request,
                        ::chat2Data::notifyService::SendVerifyCodeResponse* response,
                        ::google::protobuf::Closure* done) override;

    void SendEmail(::google::protobuf::RpcController* controller,
                   const ::chat2Data::notifyService::SendEmailRequest* request,
                   ::chat2Data::notifyService::SendEmailResponse* response,
                   ::google::protobuf::Closure* done) override;

private:
    /**
     * @brief 填充成功响应
     * @param response 响应对象
     * @note 成功时只设置错误码，不设置错误描述
     */
    template <typename Response>
    void fillSuccess(Response* response) {
        response->set_error_code(static_cast<int>(chat2Data::ErrorCode::SUCCESS));
    }

    /**
     * @brief 填充错误响应
     * @param response 响应对象
     * @param code     错误码
     */
    template <typename Response>
    void fillError(Response* response, chat2Data::ErrorCode code) {
        response->set_error_code(static_cast<int>(code));
        response->set_error_msg(chat2Data::error2String(code));
    }

private:
    std::shared_ptr<NotifyBusiness> _notifyBusiness; // 通知业务类
};

} // namespace notifyService
