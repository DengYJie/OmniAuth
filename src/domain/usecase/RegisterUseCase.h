#pragma once

#include "domain/repository/UserRepository.h"
#include <functional>
#include <memory>

/**
 * @brief 用户注册用例
 */
class RegisterUseCase {
public:
    explicit RegisterUseCase(std::shared_ptr<UserRepository> userRepository);

    void registerUserAsync(const QString& username, const QString& password,
                           const QString& email, const QString& phone,
                           std::function<void(bool, QString)> callback);
    void accountExistsAsync(const QString& account,
                            std::function<void(bool exists)> callback);

private:
    std::shared_ptr<UserRepository> m_userRepository;
};
