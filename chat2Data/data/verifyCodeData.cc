#include "verifyCodeData.h"

#include <chrono>
#include <string>
#include <utility>

#include <bite_scaffold/log.h>
#include <bite_scaffold/util.h>
#include <sw/redis++/redis.h>

#include "errorHandler.h"

namespace userService {

using chat2Data::Chat2DataException;
using chat2Data::ErrorCode;

namespace {

// 验证码缓存 key 前缀
constexpr const char* VERIFY_CODE_KEY_PREFIX = "verifyCode:";

// 验证码缓存过期时间，5 分钟
constexpr long long VERIFY_CODE_TTL_SECONDS = 5 * 60;

// 验证码缓存 JSON 字段名
constexpr const char* CODE_JSON_FIELD_CODE_ID = "codeId";
constexpr const char* CODE_JSON_FIELD_CODE = "verifyCode";
constexpr const char* CODE_JSON_FIELD_EMAIL = "email";
constexpr const char* CODE_JSON_FIELD_CREATE_TIME = "createTime";

/**
 * @brief 拼接验证码缓存的 key
 * @param codeId 验证码id
 * @return 完整缓存 key
 */
std::string makeVerifyCodeKey(const std::string& codeId) {
    return std::string(VERIFY_CODE_KEY_PREFIX) + codeId;
}

} // namespace

VerifyCodeData::VerifyCodeData(std::shared_ptr<sw::redis::Redis> redis) : _redis(std::move(redis)) {}

bool VerifyCodeData::saveVerifyCode(const std::string& codeId, const std::string& code, const std::string& email,
                                    std::time_t createTime) {
    try {
        // 1. 序列化为 JSON 字符串
        Json::Value codeJson;
        codeJson[CODE_JSON_FIELD_CODE_ID] = codeId;
        codeJson[CODE_JSON_FIELD_CODE] = code;
        codeJson[CODE_JSON_FIELD_EMAIL] = email;
        codeJson[CODE_JSON_FIELD_CREATE_TIME] = std::to_string(static_cast<long long>(createTime));

        std::optional<std::string> jsonStr = biteutil::JSON::serialize(codeJson);
        if (!jsonStr.has_value()) {
            ERR("序列化验证码缓存失败, codeId: {}", codeId);
            throw Chat2DataException(ErrorCode::USER_SAVE_VERIFY_CODE_FAILED);
        }

        // 2. 写入 Redis，TTL 5 分钟
        _redis->setex(makeVerifyCodeKey(codeId), std::chrono::seconds(VERIFY_CODE_TTL_SECONDS), jsonStr.value());
    } catch (const sw::redis::Error& e) {
        ERR("保存验证码缓存失败, codeId: {}, 原因: {}", codeId, e.what());
        throw Chat2DataException(ErrorCode::USER_SAVE_VERIFY_CODE_FAILED);
    }
    return true;
}

std::optional<VerifyCodeInfo> VerifyCodeData::getVerifyCode(const std::string& codeId) {
    std::optional<std::string> jsonStr;
    try {
        jsonStr = _redis->get(makeVerifyCodeKey(codeId));
    } catch (const std::exception& e) {
        ERR("读取验证码缓存失败, codeId: {}, 原因: {}", codeId, e.what());
        throw Chat2DataException(ErrorCode::USER_CACHE_ERROR);
    }

    // key 不存在或已被 TTL 清理时返回空
    if (!jsonStr.has_value() || jsonStr->empty()) {
        return std::nullopt;
    }

    std::optional<Json::Value> codeJson = biteutil::JSON::unserialize(jsonStr.value());
    if (!codeJson.has_value() || !codeJson->isObject()) {
        WRN("验证码缓存反序列化失败, codeId: {}", codeId);
        return std::nullopt;
    }

    const Json::Value& value = codeJson.value();
    if (!value.isMember(CODE_JSON_FIELD_CODE_ID) || !value.isMember(CODE_JSON_FIELD_CODE) ||
        !value.isMember(CODE_JSON_FIELD_EMAIL) || !value.isMember(CODE_JSON_FIELD_CREATE_TIME)) {
        WRN("验证码缓存字段不完整, codeId: {}", codeId);
        return std::nullopt;
    }

    VerifyCodeInfo info;
    info.codeId = value[CODE_JSON_FIELD_CODE_ID].asString();
    info.code = value[CODE_JSON_FIELD_CODE].asString();
    info.email = value[CODE_JSON_FIELD_EMAIL].asString();
    try {
        info.createTime = static_cast<std::time_t>(std::stoll(value[CODE_JSON_FIELD_CREATE_TIME].asString()));
    } catch (const std::exception& e) {
        WRN("验证码创建时间解析失败, codeId: {}, 原因: {}", codeId, e.what());
        info.createTime = 0;
    }
    return info;
}

bool VerifyCodeData::deleteVerifyCode(const std::string& codeId) {
    try {
        const long long removed = _redis->del(makeVerifyCodeKey(codeId));
        if (removed == 0) {
            INF("验证码不存在或已过期, codeId: {}", codeId);
            return false;
        }
    } catch (const std::exception& e) {
        // 删除缓存失败只记录日志，不影响已完成的登录流程
        WRN("删除验证码缓存失败, codeId: {}, 原因: {}", codeId, e.what());
        return false;
    }
    return true;
}

} // namespace userService
