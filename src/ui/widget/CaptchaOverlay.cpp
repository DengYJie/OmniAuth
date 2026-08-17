#include "CaptchaOverlay.h"

#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <expected>

#include <FluentQt/Foundation.h>

#include <components/foundation/overlay/OverlayScrim.h>

#include "SliderCaptchaWidget.h"
#include "domain/usecase/CaptchaService.h"
#include "data/di/AppContainer.h"

namespace fluent_b = fluent::basicinput;
namespace fluent_ly = fluent::layout;
namespace fluent_ov = fluent::overlay;

CaptchaOverlay::CaptchaOverlay(QWidget* parent) : QWidget(parent) {
  hide();
  setFocusPolicy(Qt::StrongFocus);

  // 全屏遮罩（OverlayScrim 自带 SourceOver 压暗，语义色驱动）
  m_scrim = new fluent_ov::OverlayScrim(this);
  m_scrim->setModalAndDim(true, true);
  m_scrim->setOpacityProgress(0.0);

  // 卡片容器（Card 自带圆角 + 阴影）
  m_card = new fluent_ly::Card(this);
  m_card->setAppearance(fluent_ly::Card::Layer);
  auto* layout = new QVBoxLayout(m_card);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_captchaWidget = new SliderCaptchaWidget(m_card);
  layout->addWidget(m_captchaWidget);

  auto* cardOpacityEffect = new QGraphicsOpacityEffect(m_card);
  cardOpacityEffect->setOpacity(0.0);
  m_card->setGraphicsEffect(cardOpacityEffect);

  m_fadeAnim = new QPropertyAnimation(cardOpacityEffect, "opacity", this);
  m_fadeAnim->setDuration(200);
  m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);

  // 卡片关闭后复位
  connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
    if (m_closing) {
      if (parentWidget()) {
        parentWidget()->removeEventFilter(this);
      }
      m_card->move(0, 0);
      m_card->graphicsEffect()->setProperty("opacity", 0.0);
      centerCard();
      hide();
    }
  });

  connect(m_captchaWidget, &SliderCaptchaWidget::closeRequested, this,
          &CaptchaOverlay::hideOverlay);

  connect(m_scrim, &fluent_ov::OverlayScrim::pressed, this,
          &CaptchaOverlay::hideOverlay);

  connect(m_captchaWidget, &SliderCaptchaWidget::refreshRequested, this,
          [this]() { loadNewCaptcha(); });

  connect(m_captchaWidget, &SliderCaptchaWidget::verificationCompleted, this,
          [this](const std::vector<TrackPoint>& trajectory, int finalX, int targetX) {
            const bool isPositionCorrect = std::abs(finalX - targetX) <= 4;

            if (auto riskService = AppContainer::behaviorRiskService()) {
              QPointer<CaptchaOverlay> weakThis(this);
              riskService->verifyTrajectoryAsync(
                  trajectory,
                  [weakThis, isPositionCorrect](std::expected<float, std::string> result) {
                    if (!weakThis) {
                      return;
                    }
                    bool isHumanLike = false;
                    if (result.has_value()) {
                      const float score = result.value();
                      qDebug() << "[BehaviorRiskService] 真人置信度:" << score;
                      isHumanLike = (score >= 0.5f);
                    } else {
                      qWarning() << "风控模型推理失败:"
                                 << QString::fromStdString(result.error());
                      isHumanLike = true;
                    }

                    QMetaObject::invokeMethod(
                        weakThis.data(),
                        [weakThis, isPositionCorrect, isHumanLike]() {
                          if (!weakThis) {
                            return;
                          }
                          if (isPositionCorrect && isHumanLike) {
                            weakThis->m_captchaWidget->showSuccess();
                            QTimer::singleShot(
                                500, weakThis.data(), [weakThis]() {
                                  if (!weakThis) {
                                    return;
                                  }
                                  weakThis->hideOverlay();
                                  emit weakThis->verified();
                                });
                          } else {
                            weakThis->m_captchaWidget->showError();
                          }
                        });
                  });
            }
          });
}

void CaptchaOverlay::setupUi() {
  // 布局已在上方构造函数构建；保留空实现以防调用。
}

void CaptchaOverlay::loadNewCaptcha() {
  if (auto captchaService = AppContainer::captchaService()) {
    QPointer<CaptchaOverlay> weakThis(this);
    captchaService->fetchCaptchaAsync([weakThis](auto result) {
      if (result.has_value() && weakThis) {
        QMetaObject::invokeMethod(weakThis.data(), [weakThis,
                                                    data = result.value()]() {
          if (weakThis) {
            weakThis->m_captchaWidget->setCaptchaData(data);
          }
        });
      }
    });
  }
}

void CaptchaOverlay::setTestCaptchaData(const CaptchaData& data) {
  m_captchaWidget->setCaptchaData(data);
}

void CaptchaOverlay::centerCard() {
  if (!m_card || !m_captchaWidget) {
    return;
  }
  const int cx = (width() - m_captchaWidget->width()) / 2;
  const int cy = (height() - m_captchaWidget->height()) / 2;
  m_card->move(cx, cy);
}

void CaptchaOverlay::showOverlay() {
  QWidget* topWin = window();
  if (topWin) {
    setParent(topWin);
    topWin->installEventFilter(this);
    move(0, 0);
    resize(topWin->size());
  }
  if (m_scrim) {
    m_scrim->setGeometry(rect());
    m_scrim->raise();
  }
  if (m_card) {
    m_card->raise();
  }

  m_captchaWidget->reset();
  loadNewCaptcha();

  centerCard();
  m_closing = false;

  if (m_scrim) {
    m_scrim->setOpacityProgress(0.0);
  }
  if (auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_card->graphicsEffect())) {
    effect->setOpacity(0.0);
  }

  show();
  raise();
  setFocus();

  if (m_scrim) {
    QPropertyAnimation* scrimAnim =
        new QPropertyAnimation(m_scrim, "opacityProgress", this);
    scrimAnim->setDuration(200);
    scrimAnim->setEasingCurve(QEasingCurve::OutCubic);
    scrimAnim->setStartValue(0.0);
    scrimAnim->setEndValue(1.0);
    scrimAnim->start(QPropertyAnimation::DeleteWhenStopped);
  }

  if (auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_card->graphicsEffect())) {
    m_fadeAnim->stop();
    m_fadeAnim->setStartValue(effect->opacity());
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();
  }
}

void CaptchaOverlay::hideOverlay() {
  if (isHidden() || m_closing) {
    return;
  }
  m_closing = true;

  if (m_scrim) {
    QPropertyAnimation* scrimAnim =
        new QPropertyAnimation(m_scrim, "opacityProgress", this);
    scrimAnim->setDuration(160);
    scrimAnim->setEasingCurve(QEasingCurve::OutQuad);
    scrimAnim->setStartValue(m_scrim->opacityProgress());
    scrimAnim->setEndValue(0.0);
    connect(scrimAnim, &QPropertyAnimation::finished, scrimAnim, &QObject::deleteLater);
    scrimAnim->start();
  }

  if (auto* effect = qobject_cast<QGraphicsOpacityEffect*>(m_card->graphicsEffect())) {
    m_fadeAnim->stop();
    m_fadeAnim->setStartValue(effect->opacity());
    m_fadeAnim->setEndValue(0.0);
    m_fadeAnim->start();
  }
}

bool CaptchaOverlay::eventFilter(QObject* watched, QEvent* event) {
  if (watched == parentWidget() && event->type() == QEvent::Resize) {
    resize(parentWidget()->size());
    if (m_scrim) {
      m_scrim->setGeometry(rect());
    }
    centerCard();
  }
  return QWidget::eventFilter(watched, event);
}

void CaptchaOverlay::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (m_scrim) {
    m_scrim->setGeometry(rect());
  }
  centerCard();
}

void CaptchaOverlay::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (m_scrim) {
    m_scrim->raise();
  }
  if (m_card) {
    m_card->raise();
  }
}

void CaptchaOverlay::hideEvent(QHideEvent* event) {
  QWidget::hideEvent(event);
  m_closing = false;
}

void CaptchaOverlay::mousePressEvent(QMouseEvent* event) {
  Q_UNUSED(event);
  // 遮罩与卡片区域由 OverlayScrim / SliderCaptchaWidget 自行处理；
  // 这里不吞掉事件，交由子控件。
}

void CaptchaOverlay::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape) {
    hideOverlay();
    return;
  }
  event->accept();
}
