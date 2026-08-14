#pragma once

#include <QPropertyAnimation>
#include <QWidget>
#include <vector>

#include <FluentQt/Foundation.h>
#include <FluentQt/Layout.h>

#include "domain/model/TrackPoint.h"
#include "domain/repository/CaptchaRepository.h"

namespace fluent::basicinput { class Button; }
namespace fluent::layout { class Card; }
namespace fluent::overlay { class OverlayScrim; }

class SliderCaptchaWidget;

/**
 * @brief 滑块验证码悬浮蒙层
 *
 * 用 OverlayScrim 全屏遮罩 + Card 卡片承载滑块验证，
 * 支持淡入/淡出动画与点击遮罩关闭。
 */
class CaptchaOverlay : public QWidget {
  Q_OBJECT

 public:
  explicit CaptchaOverlay(QWidget* parent = nullptr);
  ~CaptchaOverlay() override = default;

  // 对外接口：与 ViewModel 解耦
  void showOverlay();
  void hideOverlay();
  [[nodiscard]] bool isOverlayVisible() const { return isVisible(); }

  // 测试接口：模拟验证码加载完成
  void setTestCaptchaData(const CaptchaData& data);

 signals:
  void verified();
  void cancelled();

 protected:
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void setupUi();
  void loadNewCaptcha();
  void centerCard();

  fluent::overlay::OverlayScrim* m_scrim = nullptr;
  fluent::layout::Card* m_card = nullptr;
  SliderCaptchaWidget* m_captchaWidget = nullptr;
  fluent::basicinput::Button* m_btnClose = nullptr;

  QPropertyAnimation* m_fadeAnim = nullptr;
  bool m_closing = false;
};
