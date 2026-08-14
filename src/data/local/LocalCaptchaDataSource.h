#pragma once

#include "domain/repository/CaptchaRepository.h"
#include <QPainterPath>

class LocalCaptchaDataSource : public CaptchaRepository {
public:
    void generateCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback) override;

    // Helper functions can be public or private
    static QPainterPath createPuzzlePath(int x, int y, int size = 42);
private:
    static QImage generateBaseBgImage(int seed);
};
