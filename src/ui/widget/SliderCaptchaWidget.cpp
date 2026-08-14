#include "SliderCaptchaWidget.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <algorithm>

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;

SliderCaptchaWidget::SliderCaptchaWidget(QWidget* parent)
    : QWidget(parent), fluent::FluentElement() {
  setFixedSize(340, 280);
  setMouseTracking(true);

  setupUi();

  m_recoilAnim = new QVariantAnimation(this);
  m_recoilAnim->setDuration(300);
  m_recoilAnim->setEasingCurve(QEasingCurve::OutCubic);
  connect(m_recoilAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& val) {
            m_sliderX = val.toInt();
            update();
          });

  m_shakeAnim = new QVariantAnimation(this);
  m_shakeAnim->setDuration(400);
  connect(m_shakeAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& val) {
            m_shakeOffset = val.toInt();
            update();
          });
}

void SliderCaptchaWidget::onThemeUpdated() { update(); }

void SliderCaptchaWidget::setupUi() {
  m_titleText = new fluent_tf::Label(QStringLiteral("安全验证"), this);
  m_titleText->setFluentTypography(Typography::FontRole::Body);
  QFont font = m_titleText->font();
  font.setBold(true);
  font.setPixelSize(13);
  m_titleText->setFont(font);

  m_btnRefresh = new fluent_b::Button(this);
  m_btnRefresh->setFluentLayout(fluent_b::Button::IconOnly);
  m_btnRefresh->setFluentStyle(fluent_b::Button::Subtle);
  m_btnRefresh->setIconGlyph(Typography::Icons::Refresh);
  m_btnRefresh->setFixedSize(28, 28);
  m_btnRefresh->setToolTip(QStringLiteral("刷新验证码"));

  m_btnClose = new fluent_b::Button(this);
  m_btnClose->setFluentLayout(fluent_b::Button::IconOnly);
  m_btnClose->setFluentStyle(fluent_b::Button::Subtle);
  m_btnClose->setIconGlyph(Typography::Icons::Dismiss);
  m_btnClose->setFixedSize(28, 28);
  m_btnClose->setToolTip(QStringLiteral("关闭"));

  connect(m_btnRefresh, &fluent_b::Button::clicked, this,
          &SliderCaptchaWidget::refreshRequested);
  connect(m_btnClose, &fluent_b::Button::clicked, this,
          &SliderCaptchaWidget::closeRequested);
}

void SliderCaptchaWidget::setCaptchaData(const CaptchaData& data) {
  m_captchaData = data;
  reset();
}

void SliderCaptchaWidget::reset() {
  m_state = State::Ready;
  m_sliderX = 0;
  m_isDragging = false;
  m_trajectory.clear();
  update();
}

void SliderCaptchaWidget::showSuccess() {
  m_state = State::Success;
  update();
}

void SliderCaptchaWidget::showError() {
  m_state = State::Error;

  m_shakeAnim->stop();
  m_shakeAnim->setStartValue(0);
  m_shakeAnim->setKeyValueAt(0.2, 10);
  m_shakeAnim->setKeyValueAt(0.4, -10);
  m_shakeAnim->setKeyValueAt(0.6, 6);
  m_shakeAnim->setKeyValueAt(0.8, -6);
  m_shakeAnim->setEndValue(0);
  m_shakeAnim->start();

  m_recoilAnim->stop();
  m_recoilAnim->setStartValue(m_sliderX);
  m_recoilAnim->setEndValue(0);
  m_recoilAnim->start();

  QTimer::singleShot(800, this, [this]() {
    if (m_state == State::Error) {
      emit refreshRequested();
    }
  });
}

QRect SliderCaptchaWidget::sliderKnobRect() const {
  int trackX = 20;
  int trackY = 224;
  return QRect(trackX + m_sliderX, trackY, 44, 36);
}

void SliderCaptchaWidget::recordTrackPoint(const QPoint& pos) {
  m_trajectory.push_back(
      {pos.x(), pos.y(), QDateTime::currentMSecsSinceEpoch()});
}

void SliderCaptchaWidget::mousePressEvent(QMouseEvent* event) {
  if (m_state == State::Ready || m_state == State::Error) {
    QRect knobRect = sliderKnobRect();
    if (knobRect.contains(event->pos())) {
      m_isDragging = true;
      m_state = State::Dragging;
      m_dragOffsetX = event->pos().x() - knobRect.x();
      m_trajectory.clear();
      recordTrackPoint(event->pos());
      update();
    }
  }
}

void SliderCaptchaWidget::mouseMoveEvent(QMouseEvent* event) {
  if (m_isDragging) {
    int maxSlideX = 300 - 44;
    int newX = event->pos().x() - 20 - m_dragOffsetX;
    m_sliderX = std::clamp(newX, 0, maxSlideX);
    recordTrackPoint(event->pos());
    update();
  }
}

void SliderCaptchaWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (m_isDragging) {
    m_isDragging = false;
    recordTrackPoint(event->pos());
    m_state = State::Verifying;
    update();

    emit verificationCompleted(m_trajectory, m_sliderX, m_captchaData.targetX);
  }
}

QSize SliderCaptchaWidget::sizeHint() const { return QSize(340, 280); }

void SliderCaptchaWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  if (m_titleText) {
    m_titleText->move(20, 10);
    m_titleText->show();
  }
  if (m_btnRefresh) {
    m_btnRefresh->move(width() - 64, 8);
  }
  if (m_btnClose) {
    m_btnClose->move(width() - 34, 8);
  }
}

void SliderCaptchaWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  const auto& colors = themeColorsRef();

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  painter.translate(m_shakeOffset, 0);

  // 1. 绘制背景卡片
  QRect cardRect(0, 0, width(), height());
  painter.setPen(colors.strokeDivider);
  painter.setBrush(colors.bgLayer);
  painter.drawRoundedRect(cardRect.adjusted(1, 1, -1, -1), 8, 8);

  // 3. 画布区域 (20, 48, 300, 160)
  QRect canvasRect(20, 48, 300, 160);
  painter.save();

  QPainterPath canvasClip;
  canvasClip.addRoundedRect(canvasRect, 8, 8);
  painter.setClipPath(canvasClip);

  // 绘制传入的大背景图 (自带缺口阴影)
  if (!m_captchaData.backgroundImage.isNull()) {
    painter.drawImage(canvasRect, m_captchaData.backgroundImage);
  }

  // 绘制传入的拼图碎片 (边距为 16px)
  if (!m_captchaData.sliderImage.isNull()) {
    int margin = 16;
    int pieceX = canvasRect.x() + m_sliderX;
    int pieceY = canvasRect.y() + m_captchaData.targetY;
    painter.drawImage(pieceX - margin, pieceY - margin,
                      m_captchaData.sliderImage);
  }

  painter.restore();

  // 4. 滑动轨道
  QRect trackRect(20, 224, 300, 36);
  painter.setPen(Qt::NoPen);

  if (m_state == State::Success) {
    painter.setBrush(colors.systemSuccess);
    painter.drawRoundedRect(trackRect, 4, 4);

    painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
    painter.setPen(colors.textOnAccent);
    painter.drawText(trackRect, Qt::AlignCenter, QStringLiteral("验证通过"));
  } else {
    painter.setBrush(colors.controlDefault);
    painter.drawRoundedRect(trackRect, 4, 4);

    if (m_sliderX > 0) {
      QRect fillRect(20, 224, m_sliderX + 22, 36);
      QColor fillColor = (m_state == State::Error)
                             ? colors.systemCritical
                             : colors.accentDefault;
      fillColor.setAlphaF(0.4);
      painter.setBrush(fillColor);
      painter.drawRoundedRect(fillRect, 4, 4);
    }
    QString hintText;
    QColor hintColor;
    if (m_state == State::Error) {
      hintText = QStringLiteral("验证失败，请重试");
      hintColor = colors.systemCritical;
    } else if (m_isDragging || m_sliderX > 0) {
      hintText = QStringLiteral("");
    } else {
      hintText = QStringLiteral("向右拖动滑块完成拼图");
      hintColor = colors.textSecondary;
    }

    if (!hintText.isEmpty()) {
      painter.setFont(themeFont(Typography::FontRole::Body).toQFont());
      painter.setPen(hintColor);
      painter.drawText(trackRect, Qt::AlignCenter, hintText);
    }

    // 5. 绘制滑块按钮 (仅在未成功时绘制)
    QRect knobRect = sliderKnobRect();
    QColor knobBgColor =
        (m_state == State::Error)
            ? colors.systemCritical
            : (m_isDragging ? colors.accentDefault : colors.bgLayer);

    painter.setPen(QPen(colors.strokeDivider, 1));
    painter.setBrush(knobBgColor);
    painter.drawRoundedRect(knobRect, 4, 4);

    QColor knobIconColor = (m_isDragging || m_state == State::Error)
                               ? Qt::white
                               : colors.textPrimary;
    painter.setPen(
        QPen(knobIconColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    int cx = knobRect.center().x();
    int cy = knobRect.center().y();

    QPainterPath arrow;
    arrow.moveTo(cx - 3, cy - 5);
    arrow.lineTo(cx + 3, cy);
    arrow.lineTo(cx - 3, cy + 5);
    painter.drawPath(arrow);
  }
}
