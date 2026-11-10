#include "userEntity.h"

namespace chat2Data {

UserEntity::UserEntity(const std::string& userId, const std::string& nickname, const std::string& email,
                       const std::string& password, UserStatus status)
    : _userId(userId), _nickname(nickname), _email(email), _password(password), _status(status) {}

std::uint64_t UserEntity::getId() const {
    return _id;
}

const std::string& UserEntity::getUserId() const {
    return _userId;
}

const std::string& UserEntity::getNickname() const {
    return _nickname;
}

const std::string& UserEntity::getEmail() const {
    return _email;
}

const std::string& UserEntity::getPassword() const {
    return _password;
}

UserStatus UserEntity::getStatus() const {
    return _status;
}

void UserEntity::setStatus(UserStatus status) {
    _status = status;
}

void UserEntity::setPassword(const std::string& password) {
    _password = password;
}

void UserEntity::setNickname(const std::string& nickname) {
    _nickname = nickname;
}

void UserEntity::setEmail(const std::string& email) {
    _email = email;
}

} // namespace chat2Data
