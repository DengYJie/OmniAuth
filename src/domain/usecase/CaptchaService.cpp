#include "domain/usecase/CaptchaService.h"

CaptchaService::CaptchaService(std::shared_ptr<CaptchaRepository> repository)
    : m_repository(std::move(repository)) {}

void CaptchaService::fetchCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback) {
    if (m_repository) {
        m_repository->generateCaptchaAsync(std::move(callback));
    } else {
        if (callback) {
            callback(std::unexpected("Captcha repository is not initialized."));
        }
    }
}
