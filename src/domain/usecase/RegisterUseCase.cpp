#include "RegisterUseCase.h"
#include "core/CryptoUtils.h"

#include <utility>

RegisterUseCase::RegisterUseCase(std::shared_ptr<UserRepository> userRepository)
    : m_userRepository(std::move(userRepository)) {}

void RegisterUseCase::registerUserAsync(const QString& username, const QString& password,
                                        const QString& email, const QString& phone,
                                        std::function<void(bool, QString)> callback) {
    m_userRepository->getUserByAccountAsync(username,
        [=, this](std::optional<UserAuthDTO> userOpt) {
            if (userOpt.has_value()) {
                callback(false, QStringLiteral("用户名已存在"));
                return;
            }
            QString pwd_hash = CryptoUtils::hashPassword(password);
            m_userRepository->createUserAsync(username, pwd_hash, email, phone,
                [callback](bool success) {
                    if (success) {
                        callback(true, QStringLiteral("注册成功"));
                    } else {
                        callback(false, QStringLiteral("数据库插入失败"));
                    }
                });
        });
}

void RegisterUseCase::accountExistsAsync(const QString& account,
                                         std::function<void(bool exists)> callback) {
    m_userRepository->getUserByAccountAsync(account.trimmed(),
        [callback](std::optional<UserAuthDTO> userOpt) {
            callback(userOpt.has_value());
        });
}
