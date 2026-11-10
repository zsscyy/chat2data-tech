#pragma once

#include <memory>

#include <brpc/server.h>

#include "common.h"
#include "errorHandler.h"
#include "userService.pb.h"

namespace userService {

class UserBusiness;

/**
 * @brief 用户子服务RPC接口实现
 *
 * 实现 10 个 RPC 接口，并补充网关章节 bugfix 追加的 IsSessionValid 接口。
 */
class UserServiceImpl : public ::chat2Data::userService::UserService {
public:
    using ptr = std::shared_ptr<UserServiceImpl>;

    /**
     * @brief 构造RPC接口实现对象
     * @param userBusiness 用户业务类
     */
    explicit UserServiceImpl(std::shared_ptr<UserBusiness> userBusiness);

    ~UserServiceImpl() override = default;

    void ValidNickname(::google::protobuf::RpcController* controller,
                       const ::chat2Data::userService::ValidNicknameRequest* request,
                       ::chat2Data::userService::ValidNicknameResponse* response,
                       ::google::protobuf::Closure* done) override;

    void ValidEmail(::google::protobuf::RpcController* controller,
                    const ::chat2Data::userService::ValidEmailRequest* request,
                    ::chat2Data::userService::ValidEmailResponse* response,
                    ::google::protobuf::Closure* done) override;

    void UserRegister(::google::protobuf::RpcController* controller,
                      const ::chat2Data::userService::UserRegisterRequest* request,
                      ::chat2Data::userService::UserRegisterResponse* response,
                      ::google::protobuf::Closure* done) override;

    void SessionLogin(::google::protobuf::RpcController* controller,
                      const ::chat2Data::userService::SessionLoginRequest* request,
                      ::chat2Data::userService::SessionLoginResponse* response,
                      ::google::protobuf::Closure* done) override;

    void PasswdLogin(::google::protobuf::RpcController* controller,
                     const ::chat2Data::userService::PasswdLoginRequest* request,
                     ::chat2Data::userService::PasswdLoginResponse* response,
                     ::google::protobuf::Closure* done) override;

    void GetCode(::google::protobuf::RpcController* controller,
                 const ::chat2Data::userService::GetCodeRequest* request,
                 ::chat2Data::userService::GetCodeResponse* response,
                 ::google::protobuf::Closure* done) override;

    void VcodeLogin(::google::protobuf::RpcController* controller,
                    const ::chat2Data::userService::VcodeLoginRequest* request,
                    ::chat2Data::userService::VcodeLoginResponse* response,
                    ::google::protobuf::Closure* done) override;

    void Logout(::google::protobuf::RpcController* controller,
                const ::chat2Data::userService::LogoutRequest* request,
                ::chat2Data::userService::LogoutResponse* response,
                ::google::protobuf::Closure* done) override;

    void GetUserInfo(::google::protobuf::RpcController* controller,
                     const ::chat2Data::userService::GetUserInfoRequest* request,
                     ::chat2Data::userService::GetUserInfoResponse* response,
                     ::google::protobuf::Closure* done) override;

    void IsSessionValid(::google::protobuf::RpcController* controller,
                        const ::chat2Data::userService::IsSessionValidRequest* request,
                        ::chat2Data::userService::IsSessionValidResponse* response,
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
    std::shared_ptr<UserBusiness> _userBusiness; // 用户业务类
};

} // namespace userService
