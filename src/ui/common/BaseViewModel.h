#pragma once

#include <QObject>
#include <functional>

/**
 * @brief ViewModel 基类模板，支持单向数据流 (UDF)
 *
 * 注意：由于 Qt MOC 不支持模板类包含 Q_OBJECT，
 * 此基类不使用 Q_OBJECT 宏，具体的 stateChanged 信号发射交由子类实现。
 */
template <typename State>
class BaseViewModel : public QObject {
 public:
  explicit BaseViewModel(QObject* parent = nullptr) : QObject(parent) {}
  virtual ~BaseViewModel() = default;

  /**
   * @brief 获取当前不可变状态
   */
  const State& state() const { return m_state; }

 protected:
  State m_state;

  /**
   * @brief 局部更新状态并触发 UI 渲染
   * @param updater 状态更新函数，传入可修改的 State 引用
   */
  void updateState(const std::function<void(State&)>& updater) {
    updater(m_state);
    emitStateChanged();
  }

  /**
   * @brief 全量覆盖状态并触发 UI 渲染
   * @param newState 新的完整状态
   */
  void updateState(const State& newState) {
    m_state = newState;
    emitStateChanged();
  }

  /**
   * @brief 子类必须实现此方法以发射自定义的 stateChanged(State) 信号
   */
  virtual void emitStateChanged() = 0;
};
