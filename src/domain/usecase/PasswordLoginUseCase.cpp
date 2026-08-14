#include "PasswordLoginUseCase.h"
#include "core/CryptoUtils.h"

#include <utility>

PasswordLoginUseCase::PasswordLoginUseCase(std::shared_ptr<UserRepository> userRepository)
    : m_userRepository(std::move(userRepository)) {}

void PasswordLoginUseCase::loginAsync(const QString& account, const QString& password,
                                      std::function<void(bool)> callback) {
    m_userRepository->getUserByAccountAsync(account,
        [password, callback](std::optional<UserAuthDTO> userOpt) {
            if (!userOpt.has_value()) {
                callback(false);
                return;
            }
            bool isAuth = CryptoUtils::verifyPassword(userOpt->pwdHash, password);
            callback(isAuth);
        });
}
