#include "LocalCaptchaDataSource.h"
#include <QPainter>
#include <QRandomGenerator>
#include <thread>

QPainterPath LocalCaptchaDataSource::createPuzzlePath(int x, int y, int size) {
    QPainterPath path;
    qreal r = size / 5.0;

    path.moveTo(x, y);

    // 上边框 (带向上的凸起)
    path.lineTo(x + size / 2.0 - r, y);
    path.arcTo(QRectF(x + size / 2.0 - r, y - r, 2 * r, 2 * r), 180, -180);
    path.lineTo(x + size, y);

    // 右边框 (带向右的凸起)
    path.lineTo(x + size, y + size / 2.0 - r);
    path.arcTo(QRectF(x + size - r, y + size / 2.0 - r, 2 * r, 2 * r), 90, -180);
    path.lineTo(x + size, y + size);

    // 下边框与左边框
    path.lineTo(x, y + size);
    path.closeSubpath();

    return path;
}

QImage LocalCaptchaDataSource::generateBaseBgImage(int seed) {
    // 2x 高清超采样：逻辑尺寸 300x160，物理分辨率 600x320
    QImage image(600, 320, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2.0);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QRandomGenerator rng(seed == 0 ? QRandomGenerator::global()->generate() : seed);

    QLinearGradient grad(0, 0, 300, 160);
    QColor c1 = QColor::fromHsv(rng.bounded(360), 160, 60);
    QColor c2 = QColor::fromHsv(rng.bounded(360), 180, 40);
    grad.setColorAt(0.0, c1);
    grad.setColorAt(1.0, c2);
    painter.fillRect(QRect(0, 0, 300, 160), grad);

    for (int i = 0; i < 4; ++i) {
        QPainterPath path;
        path.moveTo(rng.bounded(300), rng.bounded(160));
        path.cubicTo(rng.bounded(300), rng.bounded(160), rng.bounded(300),
                     rng.bounded(160), rng.bounded(300), rng.bounded(160));
        painter.setPen(QPen(QColor(255, 255, 255, rng.bounded(40, 100)), rng.bounded(2, 6)));
        painter.drawPath(path);
    }

    painter.setPen(Qt::NoPen);
    for (int i = 0; i < 40; ++i) {
        painter.setBrush(QColor(255, 255, 255, rng.bounded(30, 80)));
        int r = rng.bounded(2, 8);
        painter.drawEllipse(rng.bounded(300), rng.bounded(160), r, r);
    }

    return image;
}

void LocalCaptchaDataSource::generateCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback) {
    std::thread([callback]() {
        CaptchaData data;
        data.targetX = QRandomGenerator::global()->bounded(130, 230);
        data.targetY = QRandomGenerator::global()->bounded(20, 90);
        data.puzzleSize = 42;
        data.captchaToken = QString::number(QRandomGenerator::global()->generate());

        QImage baseImg = generateBaseBgImage(0);
        QPainterPath puzzlePath = createPuzzlePath(data.targetX, data.targetY, data.puzzleSize);

        // 1. 生成碎片 (留足 16px 边距防凸起被裁切，2x 超采样)
        int margin = 16;
        int pieceLogicalSize = data.puzzleSize + 2 * margin;  // 42 + 32 = 74
        data.sliderImage = QImage(pieceLogicalSize * 2, pieceLogicalSize * 2, QImage::Format_ARGB32_Premultiplied);
        data.sliderImage.setDevicePixelRatio(2.0);
        data.sliderImage.fill(Qt::transparent);

        QPainter piecePainter(&data.sliderImage);
        piecePainter.setRenderHint(QPainter::Antialiasing);
        piecePainter.setRenderHint(QPainter::SmoothPixmapTransform);

        QPainterPath localPath = createPuzzlePath(margin, margin, data.puzzleSize);
        piecePainter.setClipPath(localPath);
        piecePainter.drawImage(QRectF(-data.targetX + margin, -data.targetY + margin, 300, 160), baseImg);

        piecePainter.setClipping(false);
        piecePainter.setPen(QPen(QColor(255, 255, 255, 220), 2));
        piecePainter.setBrush(Qt::NoBrush);
        piecePainter.drawPath(localPath);

        // 2. 生成带有缺口遮罩的背景图
        data.backgroundImage = baseImg;
        QPainter bgPainter(&data.backgroundImage);
        bgPainter.setRenderHint(QPainter::Antialiasing);
        bgPainter.setRenderHint(QPainter::SmoothPixmapTransform);
        bgPainter.fillPath(puzzlePath, QColor(0, 0, 0, 160));
        bgPainter.setPen(QPen(QColor(255, 255, 255, 80), 1.5));
        bgPainter.drawPath(puzzlePath);

        if (callback) {
            callback(data);
        }
    }).detach();
}
