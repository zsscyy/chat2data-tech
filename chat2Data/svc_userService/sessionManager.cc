#include "sessionManager.h"

#include <optional>
#include <utility>

#include <bite_scaffold/log.h>

#include "errorHandler.h"
#include "sessionData.h"
#include "sessionEntity.h"
#include "utils.h"

namespace userService {

SessionManager::SessionManager(std::shared_ptr<SessionData> sessionData) : _sessionData(std::move(sessionData)) {}

std::string SessionManager::createSession(const std::string& userId) {
    const std::string sessionId = chat2Data::Utils::generateUuid();
    if (!_sessionData->insertSession(sessionId, userId)) {
        ERR("创建会话失败, userId: {}", userId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_CREATE_ERROR);
    }
    INF("会话创建成功, userId: {}, sessionId: {}", userId, sessionId);
    return sessionId;
}

bool SessionManager::deleteSession(const std::string& sessionId) {
    if (!_sessionData->deleteSession(sessionId)) {
        ERR("删除会话失败, sessionId: {}", sessionId);
        throw chat2Data::Chat2DataException(chat2Data::ErrorCode::USER_SESSION_DELETE_ERROR);
    }
    INF("会话删除成功, sessionId: {}", sessionId);
    return true;
}

std::string SessionManager::getUserIdBySessionId(const std::string& sessionId) {
    std::optional<chat2Data::SessionEntity> session = _sessionData->getSessionById(sessionId);
    if (!session.has_value()) {
        DBG("会话不存在或已过期, sessionId: {}", sessionId);
        return std::string();
    }
    return session->getUserId();
}

} // namespace userService
