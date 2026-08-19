#pragma once

#include <FluentQt/Foundation.h>
#include <QString>
#include <QStringList>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>

namespace ui::widget {

    /**
     * @brief 步骤条项数据结构
     */
    struct StepItem {
        QString title;         ///< 步骤主标题
        QString subtitle;      ///< 步骤副标题（可选，为空时隐藏该行）
        bool hasError = false; ///< 步骤是否处于错误状态
    };

    /**
     * @brief 基于 Fluent Design 2 规范的横向步骤指示器 (Stepper)
     *
     * 这是一个强定制化的纯绘制组件，负责渲染进度节点与连线，支持：
     * 1. Fluent Design 主题颜色自动热切换 (Light/Dark/Contrast)
     * 2. 进度平滑插值动效与连线填充
     * 3. 键盘焦点环 (Focus-visible) 与辅助功能 (A11y) 状态播报
     * 4. 任意长文本的安全截断与 RTL 自动镜像布局
     *
     * @note 组件为弱状态容器，状态切换请由外层 ViewModel 的 stateChanged 事件单向驱动。
     */
    class Stepper : public QWidget, public fluent::FluentElement {
        Q_OBJECT
    public:
        explicit Stepper(QWidget* parent = nullptr);
        ~Stepper() override;

        /**
         * @brief 追加单个新步骤
         * @param title 步骤主标题
         * @param subtitle 步骤副标题（可选，如果为空则对应节点下不显示副标题行）
         */
        void addStep(const QString& title, const QString& subtitle = QString());

        /**
         * @brief 批量设置纯标题步骤，会清空已有步骤集合
         * @param steps 纯字符串标题列表
         */
        void setSteps(const QStringList& steps);

        /**
         * @brief 更新指定步骤的副标题
         * @param index 步骤索引
         * @param subtitle 新的副标题内容
         */
        void setSubtitle(int index, const QString& subtitle);

        /**
         * @brief 切换当前正在进行的步骤（驱动连线动画）
         * @param index 目标步骤索引，自动 Clamp 到合理区间
         */
        void setCurrentStep(int index);

        /**
         * @brief 设置或清除指定步骤的红叉截停报错状态
         * @param index 步骤索引
         * @param hasError 设为 true 将高亮错误颜色并渲染警告图标
         */
        void setError(int index, bool hasError);

        /**
         * @brief 启用或关闭鼠标/键盘点击选中反馈
         * @param clickable 开启后出现 Hand 光标、Hover 反馈并允许发射 stepClicked 信号
         */
        void setStepsClickable(bool clickable);

        /**
         * @brief 是否显隐文字标签（标题与副标题）
         * @param visible 为 false 将只渲染节点指示器与连线
         */
        void setLabelsVisible(bool visible);

        /**
         * @brief 全局开关内置的过渡动效
         * @param animated 默认 true。设为 false 时 setCurrentStep 为瞬间生硬切换
         */
        void setAnimated(bool animated);

        /**
         * @brief 清空组件的所有步骤状态与内容
         */
        void clear();

    signals:
        /**
         * @brief 当组件被设为可点击时，触发点击反馈信号
         * @param index 被点击的目标步骤索引
         */
        void stepClicked(int index);

    protected:
        void paintEvent(QPaintEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        QSize sizeHint() const override;
        QSize minimumSizeHint() const override;

        void onThemeUpdated() override;

    private slots:
        void onAnimationValueChanged(const QVariant& value);

    private:
        int hitTest(const QPoint& pos) const;
        void updateAccessibleState();

        QVector<StepItem> m_steps;
        int m_currentStep = 0;
        int m_hoveredStep = -1;
        int m_pressedStep = -1;
        int m_focusIndex = 0;

        bool m_clickable = false;
        bool m_labelsVisible = true;
        bool m_animated = true;
        bool m_focused = false;

        qreal m_progressValue = 0.0;
        QVariantAnimation* m_animation = nullptr;
    };

} // namespace ui::widget
