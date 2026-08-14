#pragma once

#include "domain/repository/UserRepository.h"
#include <functional>
#include <memory>

/**
 * @brief 密码重置用例
 */
class ResetPasswordUseCase {
public:
    explicit ResetPasswordUseCase(std::shared_ptr<UserRepository> userRepository);

    void resetPasswordAsync(const QString& account, const QString& newPassword,
                            std::function<void(bool, QString)> callback);

private:
    std::shared_ptr<UserRepository> m_userRepository;
};
