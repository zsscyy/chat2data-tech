#include "notifyServiceImpl.h"

#include <string>
#include <utility>

#include <bite_scaffold/log.h>

#include "notifyBusiness.h"

namespace notifyService {

NotifyServiceImpl::NotifyServiceImpl(std::shared_ptr<NotifyBusiness> notifyBusiness)
    : _notifyBusiness(std::move(notifyBusiness)) {}

void NotifyServiceImpl::SendVerifyCode(::google::protobuf::RpcController* controller,
                                       const ::chat2Data::notifyService::SendVerifyCodeRequest* request,
                                       ::chat2Data::notifyService::SendVerifyCodeResponse* response,
                                       ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->email().empty()) {
        WRN("发送验证码邮件参数非法，收件人邮箱为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::NOTIFY_EMAIL_INVALID);
        return;
    }
    if (request->code().empty()) {
        WRN("发送验证码邮件参数非法，验证码为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::NOTIFY_CODE_INVALID);
        return;
    }

    try {
        // 3. 业务逻辑处理：构造验证码邮件任务并入队
        _notifyBusiness->sendVerifyCodeEmail(request->email(), request->code());
        // 4. 设置RPC响应：受理成功
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("发送验证码邮件失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("发送验证码邮件发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }
}

void NotifyServiceImpl::SendEmail(::google::protobuf::RpcController* controller,
                                  const ::chat2Data::notifyService::SendEmailRequest* request,
                                  ::chat2Data::notifyService::SendEmailResponse* response,
                                  ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->to_email().empty()) {
        WRN("发送普通邮件参数非法，收件人邮箱为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::NOTIFY_EMAIL_INVALID);
        return;
    }
    if (request->subject().empty()) {
        WRN("发送普通邮件参数非法，邮件主题为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::NOTIFY_SUBJECT_INVALID);
        return;
    }
    if (request->content().empty()) {
        WRN("发送普通邮件参数非法，邮件内容为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::NOTIFY_PARAM_EMPTY);
        return;
    }

    try {
        // 3. 业务逻辑处理：构造普通邮件任务并入队
        _notifyBusiness->sendNormalEmail(request->to_email(), request->subject(), request->content());
        // 4. 设置RPC响应：受理成功
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        ERR("发送普通邮件失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("发送普通邮件发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::NOTIFY_SEND_FAILED);
    }
}

} // namespace notifyService
