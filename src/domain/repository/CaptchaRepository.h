#pragma once
#include <functional>
#include <expected>
#include <QString>
#include <QImage>

struct CaptchaData {
    QImage backgroundImage; // 包含缺口的背景大图
    QImage sliderImage;     // 滑块切片图
    int targetX = 0;        // 目标 X 坐标
    int targetY = 0;        // 目标 Y 坐标
    QString captchaToken;   // 验证流水号/Token
    int puzzleSize = 42;    // 拼图尺寸 (可以保留，或者根据需求移除)
};

class CaptchaRepository {
public:
    virtual ~CaptchaRepository() = default;
    virtual void generateCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback) = 0;
};
