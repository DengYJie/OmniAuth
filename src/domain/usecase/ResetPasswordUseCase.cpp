#include "ResetPasswordUseCase.h"
#include "core/CryptoUtils.h"

#include <utility>

ResetPasswordUseCase::ResetPasswordUseCase(std::shared_ptr<UserRepository> userRepository)
    : m_userRepository(std::move(userRepository)) {}

void ResetPasswordUseCase::resetPasswordAsync(const QString& account, const QString& newPassword,
                                              std::function<void(bool, QString)> callback) {
    m_userRepository->getUserByAccountAsync(account,
        [=, this](std::optional<UserAuthDTO> userOpt) {
            if (!userOpt.has_value()) {
                callback(false, QStringLiteral("未找到该账户"));
                return;
            }
            QString pwd_hash = CryptoUtils::hashPassword(newPassword);
            m_userRepository->updatePasswordAsync(userOpt->user.uid(), pwd_hash,
                [callback](bool success) {
                    if (success) {
                        callback(true, QStringLiteral("重置密码成功"));
                    } else {
                        callback(false, QStringLiteral("更新数据库失败"));
                    }
                });
        });
}
