#pragma once

#include "domain/repository/UserRepository.h"
#include <functional>
#include <memory>

/**
 * @brief 密码登录用例
 */
class PasswordLoginUseCase {
public:
    explicit PasswordLoginUseCase(std::shared_ptr<UserRepository> userRepository);

    void loginAsync(const QString& account, const QString& password,
                    std::function<void(bool success, int uid, const QString& username)> callback);

private:
    std::shared_ptr<UserRepository> m_userRepository;
};
