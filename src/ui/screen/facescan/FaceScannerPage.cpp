#include "FaceScannerPage.h"

#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

#include "ui/widget/FaceScannerWidget.h"
#include "ui/screen/facescan/FaceScannerViewModel.h"

FaceScannerPage::FaceScannerPage(FaceScannerViewModel* viewModel,
                                 QWidget* parent)
    : QWidget(parent), m_viewModel(viewModel) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_scannerWidget = new FaceScannerWidget(this);
  layout->addWidget(m_scannerWidget, 0, Qt::AlignCenter);

  if (m_viewModel) {
    bindViewModel();
    renderState(m_viewModel->state());
  }
}

void FaceScannerPage::bindViewModel() {
  connect(m_viewModel, &FaceScannerViewModel::stateChanged, this,
          &FaceScannerPage::renderState);
  connect(m_viewModel, &FaceScannerViewModel::frameReceived, m_scannerWidget,
          &FaceScannerWidget::setFrame);
}

void FaceScannerPage::renderState(const FaceScannerState& state) {
  if (state.isScanning || state.scanSuccess) {
    m_scannerWidget->setScanState(state.scanState, state.message);
  } else {
    m_scannerWidget->setScanState(FaceScannerWidget::ScanState::Connecting,
                                  QStringLiteral("请正对屏幕"));
  }
}
