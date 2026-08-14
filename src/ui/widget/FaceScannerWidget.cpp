#include "FaceScannerWidget.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

FaceScannerWidget::FaceScannerWidget(QWidget* parent)
    : QWidget(parent), fluent::FluentElement() {
  setMinimumSize(240, 280);

  m_currentColor = breathColorForState(ScanState::Connecting);
  m_statusMessage = QStringLiteral("正在初始化设备...");

  setupAnimations();
  applyTokenForState(ScanState::Connecting);
}

FaceScannerWidget::~FaceScannerWidget() {}

void FaceScannerWidget::onThemeUpdated() {
  applyTokenForState(m_state);
}

QColor FaceScannerWidget::breathColorForState(ScanState state) const {
  const bool dark =
      fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark;
  const auto& colors = fluent::ThemeRegistry::instance().colors(dark);
  switch (state) {
    case ScanState::Connecting:  // 启动中：中性灰
      return colors.grey130;
    case ScanState::Scanning:    // 扫描中：联动主题主色
      return colors.accentDefault;
    case ScanState::Verifying:   // 比对中：信息青
      return colors.systemInfo;
    case ScanState::Success:     // 成功：系统绿
      return colors.systemSuccess;
    case ScanState::Error:       // 失败：系统红
    default:
      return colors.systemCritical;
  }
}

void FaceScannerWidget::setupAnimations() {
  m_scanGroup = new QParallelAnimationGroup(this);

  // 1. 旋转角度动画
  m_angleAnim = new QVariantAnimation(m_scanGroup);
  m_angleAnim->setStartValue(90.0);
  m_angleAnim->setEndValue(-270.0);
  m_angleAnim->setDuration(2000);
  m_angleAnim->setLoopCount(-1);
  connect(m_angleAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            m_startAngle = value.toReal();
            update();
          });

  // 2. 弧线伸缩顺序动画组
  auto* spanSeqGroup = new QSequentialAnimationGroup(m_scanGroup);
  spanSeqGroup->setLoopCount(-1);

  auto* shrinkAnim = new QVariantAnimation(spanSeqGroup);
  shrinkAnim->setStartValue(360.0);
  shrinkAnim->setEndValue(90.0);
  shrinkAnim->setDuration(1000);
  shrinkAnim->setEasingCurve(QEasingCurve::InOutSine);
  connect(shrinkAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            if (m_state == ScanState::Scanning) {
              m_spanAngle = value.toReal();
              update();
            }
          });

  auto* expandAnim = new QVariantAnimation(spanSeqGroup);
  expandAnim->setStartValue(90.0);
  expandAnim->setEndValue(360.0);
  expandAnim->setDuration(1000);
  expandAnim->setEasingCurve(QEasingCurve::InOutSine);
  connect(expandAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            if (m_state == ScanState::Scanning) {
              m_spanAngle = value.toReal();
              update();
            }
          });

  spanSeqGroup->addAnimation(shrinkAnim);
  spanSeqGroup->addAnimation(expandAnim);

  m_scanGroup->addAnimation(m_angleAnim);
  m_scanGroup->addAnimation(spanSeqGroup);

  // 3. 呼吸动画
  m_breathAnim = new QVariantAnimation(this);
  m_breathAnim->setStartValue(1.0);
  m_breathAnim->setEndValue(0.4);
  m_breathAnim->setDuration(1200);
  m_breathAnim->setEasingCurve(QEasingCurve::InOutSine);
  m_breathAnim->setLoopCount(-1);
  connect(m_breathAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            m_opacity = value.toReal();
            update();
          });

  // 4. 颜色过渡动画
  m_colorTransitionAnim = new QVariantAnimation(this);
  m_colorTransitionAnim->setDuration(300);
  connect(m_colorTransitionAnim, &QVariantAnimation::valueChanged, this,
          [this](const QVariant& value) {
            m_currentColor = value.value<QColor>();
            update();
          });

  // 5. 外发光脉冲扩散动画 (Success 触发)
  m_pulseGroup = new QParallelAnimationGroup(this);

  m_pulseRadiusAnim = new QPropertyAnimation(this, "pulseRadius", m_pulseGroup);
  m_pulseRadiusAnim->setDuration(600);
  m_pulseRadiusAnim->setStartValue(0.0);
  m_pulseRadiusAnim->setEndValue(24.0);
  m_pulseRadiusAnim->setEasingCurve(QEasingCurve::OutQuad);

  m_pulseOpacityAnim = new QPropertyAnimation(this, "pulseOpacity", m_pulseGroup);
  m_pulseOpacityAnim->setDuration(600);
  m_pulseOpacityAnim->setStartValue(0.7);
  m_pulseOpacityAnim->setEndValue(0.0);
  m_pulseOpacityAnim->setEasingCurve(QEasingCurve::OutQuad);

  m_pulseGroup->addAnimation(m_pulseRadiusAnim);
  m_pulseGroup->addAnimation(m_pulseOpacityAnim);
}

void FaceScannerWidget::applyTokenForState(ScanState state) {
  m_state = state;

  // 呼吸灯 token：{颜色, 周期ms, 最低透明度, 最高透明度, 是否脉冲}
  struct BreathToken {
    QColor color;
    int durationMs;
    qreal minOpacity;
    qreal maxOpacity;
    bool isPulse;
  };
  BreathToken token;
  switch (state) {
    case ScanState::Connecting:  // 启动中 - 500ms 快呼吸
      token = {breathColorForState(ScanState::Connecting), 500, 0.3, 0.8, false};
      break;
    case ScanState::Scanning:    // 扫描中 - 1500ms 舒缓呼吸
      token = {breathColorForState(ScanState::Scanning), 1500, 0.3, 1.0, false};
      break;
    case ScanState::Verifying:   // 比对中 - 300ms 极速呼吸
      token = {breathColorForState(ScanState::Verifying), 300, 0.6, 1.0, false};
      break;
    case ScanState::Success:     // 成功 - 常亮 + 扩散脉冲
      token = {breathColorForState(ScanState::Success), 0, 1.0, 1.0, true};
      break;
    case ScanState::Error:       // 失败
    default:
      token = {breathColorForState(ScanState::Error), 400, 0.8, 1.0, false};
      break;
  }

  // 过渡颜色
  m_colorTransitionAnim->stop();
  m_colorTransitionAnim->setStartValue(m_currentColor);
  m_colorTransitionAnim->setEndValue(token.color);
  m_colorTransitionAnim->start();

  // 配置呼吸动效
  m_breathAnim->stop();
  if (token.durationMs > 0) {
    m_breathAnim->setDuration(token.durationMs);
    m_breathAnim->setStartValue(token.maxOpacity);
    m_breathAnim->setEndValue(token.minOpacity);
    m_breathAnim->start();
  } else {
    m_opacity = token.maxOpacity;
  }

  // 配置旋转与弧长
  if (state == ScanState::Connecting) {
    m_angleAnim->setDuration(500);
    m_spanAngle = 270.0;
    if (m_scanGroup->state() != QAbstractAnimation::Running) {
      m_scanGroup->start();
    }
  } else if (state == ScanState::Scanning) {
    m_angleAnim->setDuration(2000);
    if (m_scanGroup->state() != QAbstractAnimation::Running) {
      m_scanGroup->start();
    }
  } else if (state == ScanState::Verifying) {
    m_angleAnim->setDuration(300);
    m_spanAngle = 90.0;
    if (m_scanGroup->state() != QAbstractAnimation::Running) {
      m_scanGroup->start();
    }
  } else {
    m_scanGroup->stop();
    m_startAngle = 90.0;
    m_spanAngle = 360.0;
  }

  // 触发脉冲外发光
  if (token.isPulse) {
    m_pulseGroup->stop();
    m_pulseGroup->start();
  }

  update();
}

void FaceScannerWidget::setScanState(ScanState state, const QString& message) {
  if (!message.isEmpty()) {
    m_statusMessage = message;
  } else {
    switch (state) {
      case ScanState::Connecting:
        m_statusMessage = QStringLiteral("正在初始化设备...");
        break;
      case ScanState::Scanning:
        m_statusMessage = QStringLiteral("正在识别面部特征...");
        break;
      case ScanState::Verifying:
        m_statusMessage = QStringLiteral("正在比对特征库...");
        break;
      case ScanState::Success:
        m_statusMessage = QStringLiteral("识别成功，欢迎回来！");
        break;
      case ScanState::Error:
      default:
        m_statusMessage = QStringLiteral("未识别到人脸，请重试");
        break;
    }
  }
  
  // 仅在状态实际发生变化时，才重新触发动画（防止同状态下疯狂闪烁）
  if (m_state != state) {
      applyTokenForState(state);
  } else {
      // 如果状态未变，只需刷新文字
      update();
  }
}

void FaceScannerWidget::startScan() {
  setScanState(ScanState::Scanning);
}

void FaceScannerWidget::stopScan() {
  setScanState(ScanState::Connecting, QStringLiteral("请正对屏幕"));
}

void FaceScannerWidget::setScanResult(bool success, const QString& message) {
  setScanState(success ? ScanState::Success : ScanState::Error, message);
}

void FaceScannerWidget::setFrame(const QImage& frame) {
  m_currentFrame = frame;
  update();
}

QSize FaceScannerWidget::sizeHint() const {
  return QSize(240, 280);
}
void FaceScannerWidget::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
}

void FaceScannerWidget::paintEvent(QPaintEvent* event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  int circleSize = qMin(width(), height() - 40);
  QRectF circleRect((width() - circleSize) / 2.0, 0, circleSize, circleSize);

  qreal penWidth = 4.0;
  circleRect.adjust(penWidth, penWidth, -penWidth, -penWidth);

  // 1. 绘制底层相机画面 (如果存在) 或 底色圆环
  if (!m_currentFrame.isNull()) {
    QPainterPath clipPath;
    clipPath.addEllipse(circleRect);
    painter.save();
    painter.setClipPath(clipPath);

    // Calculate aspect fill rect
    QSize imgSize = m_currentFrame.size();
    qreal imgRatio = static_cast<qreal>(imgSize.width()) / imgSize.height();
    qreal targetRatio = circleRect.width() / circleRect.height();
    QRectF targetRect = circleRect;

    if (imgRatio > targetRatio) {
      qreal newW = circleRect.height() * imgRatio;
      targetRect.setX(circleRect.x() - (newW - circleRect.width()) / 2.0);
      targetRect.setWidth(newW);
    } else {
      qreal newH = circleRect.width() / imgRatio;
      targetRect.setY(circleRect.y() - (newH - circleRect.height()) / 2.0);
      targetRect.setHeight(newH);
    }

    painter.drawImage(targetRect, m_currentFrame);
    painter.restore();
  } else {
    painter.setPen(Qt::NoPen);
    painter.setBrush(themeColorsRef().bgLayer);
    painter.drawEllipse(circleRect);
  }
  // 2. 绘制外发光扩散脉冲 (Success 状态)
  if (m_pulseOpacity > 0.0) {
    QRectF pulseRect = circleRect.adjusted(-m_pulseRadius, -m_pulseRadius,
                                           m_pulseRadius, m_pulseRadius);
    QColor pulseColor = m_currentColor;
    pulseColor.setAlphaF(m_pulseOpacity);
    QPen pulsePen(pulseColor, penWidth);
    pulsePen.setCapStyle(Qt::RoundCap);
    painter.setPen(pulsePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pulseRect);
  }

  // 3. 绘制主动态弧线/圆环
  QPen ringPen;
  QColor c = m_currentColor;
  c.setAlphaF(m_opacity);
  ringPen.setColor(c);
  ringPen.setWidthF(penWidth);
  ringPen.setCapStyle(Qt::RoundCap);
  painter.setPen(ringPen);
  painter.setBrush(Qt::NoBrush);

  painter.drawArc(circleRect, static_cast<int>(m_startAngle * 16),
                  static_cast<int>(m_spanAngle * 16));

  // 4. 绘制状态文本
  QRect textRect(0, circleSize, width(), height() - circleSize);
  QFont font = themeFont(Typography::FontRole::Body).toQFont();
  painter.setFont(font);
  painter.setPen(m_currentColor);
  painter.drawText(textRect, Qt::AlignCenter, m_statusMessage);
}
