#pragma once

#include <QDateTime>
#include <QImage>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QWidget>
#include <vector>

#include <FluentQt/BasicInput.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/TextFields.h>

#include "domain/model/TrackPoint.h"
#include "domain/repository/CaptchaRepository.h"

namespace fluent::basicinput { class Button; }
namespace fluent::textfields { class Label; }

/**
 * @brief 滑块验证码
 */
class SliderCaptchaWidget : public QWidget, public fluent::FluentElement {
  Q_OBJECT
 public:
  enum class State { Ready, Dragging, Verifying, Success, Error };

  explicit SliderCaptchaWidget(QWidget* parent = nullptr);
  ~SliderCaptchaWidget() override = default;

  void setCaptchaData(const CaptchaData& data);
  void reset();
  void showSuccess();
  void showError();

  State state() const { return m_state; }

  QSize sizeHint() const override;

  // FluentElement: 全局主题变化时刷新 token 派生的绘制
  void onThemeUpdated() override;

 signals:
  void verificationCompleted(const std::vector<TrackPoint>& trajectory,
                             int finalX, int targetX);
  void refreshRequested();
  void closeRequested();
 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
 private:
  void setupUi();
  QRect sliderKnobRect() const;
  void recordTrackPoint(const QPoint& pos);

 private:
  State m_state = State::Ready;

  int m_sliderX = 0;
  int m_dragOffsetX = 0;
  bool m_isDragging = false;
  int m_shakeOffset = 0;

  CaptchaData m_captchaData;
  std::vector<TrackPoint> m_trajectory;

  fluent::basicinput::Button* m_btnRefresh = nullptr;
  fluent::basicinput::Button* m_btnClose = nullptr;
  fluent::textfields::Label* m_titleText = nullptr;

  QVariantAnimation* m_recoilAnim = nullptr;
  QVariantAnimation* m_shakeAnim = nullptr;
};
