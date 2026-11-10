#pragma once

#include <ctime>
#include <memory>
#include <optional>
#include <string>

namespace sw {
namespace redis {
class Redis;
} // namespace redis
} // namespace sw

namespace userService {

/**
 * @brief 验证码信息
 */
struct VerifyCodeInfo {
    std::string codeId;     // 验证码id
    std::string code;       // 验证码内容
    std::string email;      // 验证码所属邮箱
    std::time_t createTime; // 验证码创建时间
};

/**
 * @brief 验证码数据访问层
 *
 * 验证码只存放于 Redis，value 为 JSON 字符串，key：verifyCode:<codeId>，TTL 5 分钟：
 *   - 保存：写入 JSON 并设置 5 分钟过期
 *   - 读取：读取 JSON，不物理删除
 *   - 使用：登录成功后直接删除 key（用后即删），未使用的由 TTL 兜底
 */
class VerifyCodeData {
public:
    using ptr = std::shared_ptr<VerifyCodeData>;

    /**
     * @brief 构造验证码数据访问层
     * @param redis Redis 客户端句柄
     */
    explicit VerifyCodeData(std::shared_ptr<sw::redis::Redis> redis);

    /**
     * @brief 保存验证码
     * @param codeId     验证码id
     * @param code       验证码内容
     * @param email      验证码所属邮箱
     * @param createTime 验证码创建时间
     * @return 保存成功返回 true
     * @throws chat2Data::Chat2DataException 序列化或 Redis 写入失败（USER_SAVE_VERIFY_CODE_FAILED）
     */
    bool saveVerifyCode(const std::string& codeId, const std::string& code, const std::string& email,
                        std::time_t createTime);

    /**
     * @brief 读取验证码
     * @param codeId 验证码id
     * @return 存在时返回验证码信息，不存在返回空
     * @throws chat2Data::Chat2DataException Redis 读取失败（USER_CACHE_ERROR）
     */
    std::optional<VerifyCodeInfo> getVerifyCode(const std::string& codeId);

    /**
     * @brief 删除验证码
     * @param codeId 验证码id
     * @return key 存在并删除成功返回 true，key 不存在返回 false
     * @note 使用成功后直接删除，物理过期仍由 Redis TTL 负责
     */
    bool deleteVerifyCode(const std::string& codeId);

private:
    std::shared_ptr<sw::redis::Redis> _redis; // 缓存句柄
};

} // namespace userService
