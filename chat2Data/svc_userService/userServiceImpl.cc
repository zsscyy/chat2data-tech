#include "userServiceImpl.h"

#include <string>
#include <utility>

#include <bite_scaffold/log.h>

#include "userBusiness.h"

namespace userService {

namespace {

// 密码最短长度，与注册参数校验保持一致
constexpr std::size_t PASSWORD_MIN_LENGTH = 6;

/**
 * @brief 校验密码格式
 * @param password 明文密码
 * @return 长度不小于6且不含空格返回 true
 */
bool isValidPassword(const std::string& password) {
    if (password.size() < PASSWORD_MIN_LENGTH) {
        return false;
    }
    return password.find(' ') == std::string::npos;
}

} // namespace

UserServiceImpl::UserServiceImpl(std::shared_ptr<UserBusiness> userBusiness) : _userBusiness(std::move(userBusiness)) {}

void UserServiceImpl::ValidNickname(::google::protobuf::RpcController* controller,
                                    const ::chat2Data::userService::ValidNicknameRequest* request,
                                    ::chat2Data::userService::ValidNicknameResponse* response,
                                    ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->nickname().empty()) {
        WRN("昵称检测参数为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_NICKNAME_EMPTY);
        return;
    }

    try {
        // 3. 业务逻辑处理：检测昵称是否唯一
        if (!_userBusiness->isNicknameUnique(request->nickname())) {
            // 4. 设置RPC响应：昵称已存在
            fillError(response, chat2Data::ErrorCode::USER_NICKNAME_EXIST);
            return;
        }
        // 4. 设置RPC响应：昵称可用
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("昵称检测失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("昵称检测发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::ValidEmail(::google::protobuf::RpcController* controller,
                                 const ::chat2Data::userService::ValidEmailRequest* request,
                                 ::chat2Data::userService::ValidEmailResponse* response,
                                 ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->email().empty()) {
        WRN("邮箱检测参数为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_EMAIL_EMPTY);
        return;
    }

    try {
        // 3. 业务逻辑处理：检测邮箱是否唯一
        if (!_userBusiness->isEmailUnique(request->email())) {
            fillError(response, chat2Data::ErrorCode::USER_EMAIL_EXIST);
            return;
        }
        // 4. 设置RPC响应：邮箱可用
        fillSuccess(response);
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("邮箱检测失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("邮箱检测发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::UserRegister(::google::protobuf::RpcController* controller,
                                  const ::chat2Data::userService::UserRegisterRequest* request,
                                  ::chat2Data::userService::UserRegisterResponse* response,
                                  ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->nickname().empty() || request->email().empty() || !isValidPassword(request->password())) {
        WRN("注册参数非法, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_REGISTER_PARAM_ERROR);
        return;
    }

    try {
        // 3. 业务逻辑处理：注册用户
        _userBusiness->registerUser(request->nickname(), request->email(), request->password());

        // 4. 设置RPC响应
        fillSuccess(response);
        INF("用户注册RPC成功, requestId: {}", request->request_id());
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("用户注册失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("用户注册发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::SessionLogin(::google::protobuf::RpcController* controller,
                                   const ::chat2Data::userService::SessionLoginRequest* request,
                                   ::chat2Data::userService::SessionLoginResponse* response,
                                   ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->session_id().empty()) {
        WRN("会话登录参数为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_SESSION_INVALID);
        return;
    }

    try {
        // 3. 业务逻辑处理：会话登录
        if (!_userBusiness->loginWithSession(request->session_id())) {
            // 4. 设置RPC响应：会话登录失败
            fillError(response, chat2Data::ErrorCode::USER_SESSION_LOGIN_FAILED);
            return;
        }

        // 4. 设置RPC响应：会话登录成功
        fillSuccess(response);
        INF("会话登录RPC成功, requestId: {}", request->request_id());
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("会话登录失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("会话登录发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::PasswdLogin(::google::protobuf::RpcController* controller,
                                  const ::chat2Data::userService::PasswdLoginRequest* request,
                                  ::chat2Data::userService::PasswdLoginResponse* response,
                                  ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->username().empty() || request->password().empty()) {
        WRN("密码登录参数为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_PASSWORD_INVALID);
        return;
    }

    try {
        // 3. 业务逻辑处理：密码登录，返回新会话id
        const std::string sessionId = _userBusiness->loginWithPassword(request->username(), request->password());

        // 4. 设置RPC响应：回填 sessionId
        response->mutable_result()->set_session_id(sessionId);
        fillSuccess(response);
        INF("密码登录RPC成功, requestId: {}", request->request_id());
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("密码登录失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("密码登录发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::GetCode(::google::protobuf::RpcController* controller,
                              const ::chat2Data::userService::GetCodeRequest* request,
                              ::chat2Data::userService::GetCodeResponse* response,
                              ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->email().empty()) {
        WRN("获取验证码参数为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_EMAIL_EMPTY);
        return;
    }

    try {
        // 3. 业务逻辑处理：生成验证码并调用通知服务发送
        const std::string codeId = _userBusiness->getVerifyCode(request->email());

        // 4. 设置RPC响应：回填 codeId
        response->mutable_result()->set_code_id(codeId);
        fillSuccess(response);
        INF("获取验证码RPC成功, requestId: {}", request->request_id());
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("获取验证码失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("获取验证码发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::VcodeLogin(::google::protobuf::RpcController* controller,
                                 const ::chat2Data::userService::VcodeLoginRequest* request,
                                 ::chat2Data::userService::VcodeLoginResponse* response,
                                 ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->email().empty() || request->verify_code().empty() || request->code_id().empty()) {
        WRN("验证码登录参数为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_PASSWORD_INVALID);
        return;
    }

    try {
        // 3. 业务逻辑处理：验证码登录，返回新会话id
        const std::string sessionId =
            _userBusiness->loginWithVerifyCode(request->email(), request->code_id(), request->verify_code());

        // 4. 设置RPC响应：回填 sessionId
        response->mutable_result()->set_session_id(sessionId);
        fillSuccess(response);
        INF("验证码登录RPC成功, requestId: {}", request->request_id());
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("验证码登录失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("验证码登录发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::Logout(::google::protobuf::RpcController* controller,
                             const ::chat2Data::userService::LogoutRequest* request,
                             ::chat2Data::userService::LogoutResponse* response,
                             ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->session_id().empty()) {
        WRN("退出登录参数为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_SESSION_INVALID);
        return;
    }

    try {
        // 3. 业务逻辑处理：退出登录
        if (!_userBusiness->logout(request->session_id())) {
            // 4. 设置RPC响应：退出登录失败
            fillError(response, chat2Data::ErrorCode::USER_LOGOUT_FAILED);
            return;
        }

        // 4. 设置RPC响应：退出登录成功
        fillSuccess(response);
        INF("退出登录RPC成功, requestId: {}", request->request_id());
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("退出登录失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("退出登录发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::GetUserInfo(::google::protobuf::RpcController* controller,
                                  const ::chat2Data::userService::GetUserInfoRequest* request,
                                  ::chat2Data::userService::GetUserInfoResponse* response,
                                  ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->session_id().empty()) {
        WRN("获取用户信息参数为空, requestId: {}", request->request_id());
        fillError(response, chat2Data::ErrorCode::USER_GET_USER_INFO_PARAM_ERROR);
        return;
    }

    try {
        // 3. 业务逻辑处理：通过会话获取用户信息
        const UserInfo userInfo = _userBusiness->getUserInfo(request->session_id());

        // 4. 设置RPC响应：回填 user_info
        auto protoUserInfo = response->mutable_result()->mutable_user_info();
        protoUserInfo->set_user_id(userInfo.userId);
        protoUserInfo->set_nickname(userInfo.nickname);
        protoUserInfo->set_email(userInfo.email);
        fillSuccess(response);
        INF("获取用户信息RPC成功, requestId: {}", request->request_id());
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("获取用户信息失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("获取用户信息发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

void UserServiceImpl::IsSessionValid(::google::protobuf::RpcController* controller,
                                     const ::chat2Data::userService::IsSessionValidRequest* request,
                                     ::chat2Data::userService::IsSessionValidResponse* response,
                                     ::google::protobuf::Closure* done) {
    // 0. 构造ClosureGuard对象，该对象以RAII机制管理done->Run()方法的调用
    brpc::ClosureGuard doneGuard(done);

    // 1. 解析RPC请求中参数
    response->set_request_id(request->request_id());

    // 2. 参数校验
    if (request->session_id().empty()) {
        WRN("会话有效性检查参数为空, requestId: {}", request->request_id());
        response->set_is_valid(false);
        fillError(response, chat2Data::ErrorCode::USER_SESSION_INVALID);
        return;
    }

    try {
        // 3. 业务逻辑处理：检查会话是否有效
        std::string userId;
        const bool valid = _userBusiness->isSessionValid(request->session_id(), userId);

        // 4. 设置RPC响应：回填 is_valid 与 user_id
        response->set_is_valid(valid);
        response->set_user_id(userId);
        fillSuccess(response);
        INF("会话有效性检查RPC成功, requestId: {}, isValid: {}", request->request_id(), valid);
    } catch (const chat2Data::Chat2DataException& e) {
        // 5. 业务层抛出异常，统一按业务处理失败逻辑处理
        ERR("会话有效性检查失败, requestId: {}, 原因: {}", request->request_id(), e.what());
        response->set_is_valid(false);
        fillError(response, e.getErrorCode());
    } catch (const std::exception& e) {
        ERR("会话有效性检查发生未知异常, requestId: {}, 原因: {}", request->request_id(), e.what());
        response->set_is_valid(false);
        fillError(response, chat2Data::ErrorCode::USER_UNKNOWN_ERROR);
    }
}

} // namespace userService
