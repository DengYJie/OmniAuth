#pragma once

#include "domain/repository/CaptchaRepository.h"

class RemoteCaptchaDataSource : public CaptchaRepository {
public:
    void generateCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback) override;
};
