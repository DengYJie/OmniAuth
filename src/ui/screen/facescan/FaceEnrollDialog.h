#pragma once

#include <QDialog>
#include "ui/screen/facescan/FaceEnrollViewModel.h"

namespace fluent::basicinput {
class Button;
}
namespace fluent::textfields {
class Label;
}
class FaceScannerWidget;

/**
 * @brief 人脸录入与绑定对话框
 */
class FaceEnrollDialog : public QDialog {
    Q_OBJECT

public:
    explicit FaceEnrollDialog(int uid, QWidget* parent = nullptr);
    ~FaceEnrollDialog() override;

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    void bindViewModel();
    void renderState(const FaceEnrollState& state);

    int m_uid = -1;
    FaceEnrollViewModel* m_viewModel = nullptr;

    FaceScannerWidget* m_scannerWidget = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::textfields::Label* m_tipLabel = nullptr;
    fluent::basicinput::Button* m_actionBtn = nullptr;
    fluent::basicinput::Button* m_cancelBtn = nullptr;
};
