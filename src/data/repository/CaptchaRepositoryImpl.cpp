#include "CaptchaRepositoryImpl.h"

CaptchaRepositoryImpl::CaptchaRepositoryImpl(std::unique_ptr<CaptchaRepository> localSource,
                                     std::unique_ptr<CaptchaRepository> remoteSource)
    : m_localSource(std::move(localSource)), m_remoteSource(std::move(remoteSource)) {}

void CaptchaRepositoryImpl::setUseRemote(bool useRemote) {
    m_useRemote = useRemote;
}

void CaptchaRepositoryImpl::generateCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback) {
    if (m_useRemote && m_remoteSource) {
        m_remoteSource->generateCaptchaAsync(std::move(callback));
    } else if (m_localSource) {
        m_localSource->generateCaptchaAsync(std::move(callback));
    } else {
        if (callback) {
            callback(std::unexpected("No valid captcha data source available."));
        }
    }
}
