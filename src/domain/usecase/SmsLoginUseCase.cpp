#include "SmsLoginUseCase.h"

#include <utility>

SmsLoginUseCase::SmsLoginUseCase(std::shared_ptr<UserRepository> userRepository)
    : m_userRepository(std::move(userRepository)) {}

void SmsLoginUseCase::smsLoginAsync(const QString& account, const QString& code,
                                    std::function<void(bool, QString)> callback) {
    if (code.trimmed().length() != 6) {
        callback(false, QStringLiteral("请输入 6 位有效验证码"));
        return;
    }
    m_userRepository->getUserByAccountAsync(account.trimmed(),
        [callback](std::optional<UserAuthDTO> userOpt) {
            if (!userOpt.has_value()) {
                callback(false, QStringLiteral("账户不存在"));
            } else {
                callback(true, QStringLiteral("登录成功"));
            }
        });
}
