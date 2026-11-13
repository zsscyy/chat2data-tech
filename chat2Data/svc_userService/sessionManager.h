#pragma once

#include <memory>
#include <string>

namespace userService {

class SessionData;

/**
 * @brief 会话管理类
 *
 * 负责会话id的生成与会话的创建、删除、有效性校验，
 * 底层读写与缓存策略由 SessionData 负责。
 */
class SessionManager {
public:
    using ptr = std::shared_ptr<SessionManager>;

    /**
     * @brief 构造会话管理类
     * @param sessionData 会话数据访问层
     */
    explicit SessionManager(std::shared_ptr<SessionData> sessionData);

    /**
     * @brief 创建会话
     * @param userId 用户id
     * @return 新生成的会话id
     * @throws chat2Data::Chat2DataException 写库失败时抛出会话创建失败
     */
    std::string createSession(const std::string& userId);

    /**
     * @brief 检测会话是否有效
     * @param sessionId 会话id
     * @return 会话存在且未过期返回 true
     */
    bool isSessionValid(const std::string& sessionId);

    /**
     * @brief 删除会话
     * @param sessionId 会话id
     * @return 删除成功返回 true
     * @throws chat2Data::Chat2DataException 写库失败时抛出会话删除失败
     */
    bool deleteSession(const std::string& sessionId);

    /**
     * @brief 通过会话id获取用户id
     * @param sessionId 会话id
     * @return 会话有效返回用户id，会话不存在返回空串
     */
    std::string getUserIdBySessionId(const std::string& sessionId);

private:
    std::shared_ptr<SessionData> _sessionData; // 会话数据访问层
};

} // namespace userService
