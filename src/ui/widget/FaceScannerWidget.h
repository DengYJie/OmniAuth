#pragma once

#include <FluentQt/Foundation.h>

#include <QColor>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QString>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

class FaceScannerWidget : public QWidget, public fluent::FluentElement {
  Q_OBJECT
  Q_PROPERTY(qreal pulseRadius READ pulseRadius WRITE setPulseRadius)
  Q_PROPERTY(qreal pulseOpacity READ pulseOpacity WRITE setPulseOpacity)

 public:
  enum class ScanState { Connecting, Scanning, Verifying, Success, Error };
  Q_ENUM(ScanState)

  explicit FaceScannerWidget(QWidget* parent = nullptr);
  ~FaceScannerWidget() override;

  ScanState state() const { return m_state; }

  qreal pulseRadius() const { return m_pulseRadius; }
  void setPulseRadius(qreal radius) {
    m_pulseRadius = radius;
    update();
  }

  qreal pulseOpacity() const { return m_pulseOpacity; }
  void setPulseOpacity(qreal opacity) {
    m_pulseOpacity = opacity;
    update();
  }

 public slots:
  void setScanState(ScanState state, const QString& message = QString());
  void startScan();
  void stopScan();
  void setScanResult(bool success, const QString& message = QString());
  void setFrame(const QImage& frame);
 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  QSize sizeHint() const override;

  // FluentElement: 全局主题变化时刷新 token 派生的呼吸灯状态
  void onThemeUpdated() override;

 private:
  void setupAnimations();
  void applyTokenForState(ScanState state);
  /// 根据 Fluent 语义色解析给定扫描状态的呼吸灯颜色
  QColor breathColorForState(ScanState state) const;

 private:
  ScanState m_state = ScanState::Connecting;
  QString m_statusMessage;

  // Animations
  QParallelAnimationGroup* m_scanGroup = nullptr;
  QVariantAnimation* m_angleAnim = nullptr;
  QVariantAnimation* m_breathAnim = nullptr;
  QVariantAnimation* m_colorTransitionAnim = nullptr;
  QPropertyAnimation* m_pulseRadiusAnim = nullptr;
  QPropertyAnimation* m_pulseOpacityAnim = nullptr;
  QParallelAnimationGroup* m_pulseGroup = nullptr;

  // Visual properties
  qreal m_startAngle = 90.0;
  qreal m_spanAngle = 360.0;
  qreal m_opacity = 1.0;
  QColor m_currentColor;

  qreal m_pulseRadius = 0.0;
  qreal m_pulseOpacity = 0.0;
  QImage m_currentFrame;
};
