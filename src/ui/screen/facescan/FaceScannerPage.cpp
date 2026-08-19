#include "FaceScannerPage.h"

#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <FluentQt/Layout.h>

#include "ui/widget/FaceScannerWidget.h"
#include "ui/screen/facescan/FaceScannerViewModel.h"

FaceScannerPage::FaceScannerPage(FaceScannerViewModel* viewModel,
                                 QWidget* parent)
    : QWidget(parent), m_viewModel(viewModel) {
  auto* pageLayout = new QVBoxLayout(this);
  pageLayout->setContentsMargins(0, 0, 0, 0);
  pageLayout->setSpacing(0);
  pageLayout->setAlignment(Qt::AlignCenter);

  auto* card = new fluent::layout::Card(this);
  card->setAppearance(fluent::layout::Card::Layer);
  card->setBorderVisible(true);
  card->setFixedSize(360, 440);

  auto* cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(28, 28, 28, 28);
  cardLayout->setSpacing(0);
  cardLayout->setAlignment(Qt::AlignCenter);

  m_scannerWidget = new FaceScannerWidget(card);
  cardLayout->addWidget(m_scannerWidget, 0, Qt::AlignCenter);

  pageLayout->addWidget(card);

  if (m_viewModel) {
    bindViewModel();
  }
}

void FaceScannerPage::bindViewModel() {
  m_viewModel->observe(this, &FaceScannerPage::renderState);
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
