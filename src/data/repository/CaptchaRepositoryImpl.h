#pragma once

#include "domain/repository/CaptchaRepository.h"
#include <memory>

class CaptchaRepositoryImpl : public CaptchaRepository {
public:
    CaptchaRepositoryImpl(std::unique_ptr<CaptchaRepository> localSource,
                      std::unique_ptr<CaptchaRepository> remoteSource);
    
    void setUseRemote(bool useRemote);
    void generateCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback);

private:
    std::unique_ptr<CaptchaRepository> m_localSource;
    std::unique_ptr<CaptchaRepository> m_remoteSource;
    bool m_useRemote = false;
};
