#include "SmsLoginUseCase.h"

#include <utility>

SmsLoginUseCase::SmsLoginUseCase(std::shared_ptr<UserRepository> userRepository)
    : m_userRepository(std::move(userRepository)) {}

void SmsLoginUseCase::smsLoginAsync(const QString& account, const QString& code,
                                    std::function<void(bool, int, const QString&, const QString&)> callback) {
    if (code.trimmed().length() != 6) {
        callback(false, -1, QString(), QStringLiteral("请输入 6 位有效验证码"));
        return;
    }
    m_userRepository->getUserByAccountAsync(account.trimmed(),
        [callback](std::optional<UserAuthDTO> userOpt) {
            if (!userOpt.has_value()) {
                callback(false, -1, QString(), QStringLiteral("账户不存在"));
            } else {
                callback(true, userOpt->user.uid(), userOpt->user.username(), QStringLiteral("登录成功"));
            }
        });
}
