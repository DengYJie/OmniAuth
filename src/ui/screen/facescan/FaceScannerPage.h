#pragma once

#include <QWidget>

class FaceScannerWidget;
class FaceScannerViewModel;
struct FaceScannerState;

/**
 * @brief 人脸识别页面 (居中展示 FaceScannerWidget)
 */
class FaceScannerPage : public QWidget {
  Q_OBJECT
 public:
  explicit FaceScannerPage(FaceScannerViewModel* viewModel,
                           QWidget* parent = nullptr);
  ~FaceScannerPage() override = default;

  FaceScannerWidget* scanner() const { return m_scannerWidget; }

 private:
  void bindViewModel();
  void renderState(const FaceScannerState& state);

  FaceScannerViewModel* m_viewModel = nullptr;
  FaceScannerWidget* m_scannerWidget = nullptr;
};
