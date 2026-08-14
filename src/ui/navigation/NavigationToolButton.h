#pragma once

#include "ui/navigation/NavigationPushButton.h"
#include <QString>

class QVariantAnimation;
class QMoveEvent;
class QShowEvent;

namespace ui::navigation {

/**
 * @brief 导航纯图标工具按钮
 *
 * 专用于顶部汉堡与返回等动作按钮，尺寸固定（48x48），图标始终几何居中。
 * 支持在导航栏排版变化时启用平滑位移动画（setAnimatedMove）。
 */
class NavigationToolButton : public NavigationPushButton {
    Q_OBJECT

public:
    explicit NavigationToolButton(const QString& iconGlyph, QWidget* parent = nullptr);
    ~NavigationToolButton() override = default;

    bool isSelectable() const { return false; }

    void setCompacted(bool compacted) override;

    /// 方向切换后保持 48x48 固定尺寸（基类按行高 setFixedHeight 会破坏锁定尺寸）。
    void setOrientation(NavigationOrientation orientation) override;

    void setAnimatedMove(bool enable) { m_animatedMove = enable; }
    bool isAnimatedMove() const { return m_animatedMove; }

protected:
    int iconDrawX() const override;
    void moveEvent(QMoveEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    bool m_animatedMove = false;
    bool m_isMovingProgrammatically = false;
    QVariantAnimation* m_slideAnimation = nullptr;
};

} // namespace ui::navigation
