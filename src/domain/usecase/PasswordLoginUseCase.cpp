#include "PasswordLoginUseCase.h"
#include "core/CryptoUtils.h"

#include <utility>

PasswordLoginUseCase::PasswordLoginUseCase(std::shared_ptr<UserRepository> userRepository)
    : m_userRepository(std::move(userRepository)) {}

void PasswordLoginUseCase::loginAsync(const QString& account, const QString& password,
                                      std::function<void(bool, int, const QString&)> callback) {
    m_userRepository->getUserByAccountAsync(account,
        [password, callback](std::optional<UserAuthDTO> userOpt) {
            if (!userOpt.has_value()) {
                callback(false, -1, QString());
                return;
            }
            bool isAuth = CryptoUtils::verifyPassword(userOpt->pwdHash, password);
            if (isAuth) {
                callback(true, userOpt->user.uid(), userOpt->user.username());
            } else {
                callback(false, -1, QString());
            }
        });
}
