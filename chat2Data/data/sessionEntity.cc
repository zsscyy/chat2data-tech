#include "sessionEntity.h"

namespace chat2Data {

SessionEntity::SessionEntity(const std::string& sessionId, const std::string& userId)
    : _sessionId(sessionId), _userId(userId) {}

std::uint64_t SessionEntity::getId() const {
    return _id;
}

const std::string& SessionEntity::getSessionId() const {
    return _sessionId;
}

const std::string& SessionEntity::getUserId() const {
    return _userId;
}

} // namespace chat2Data
