#include "FaceEnrollDialog.h"
#include "ui/widget/FaceScannerWidget.h"
#include "design/Spacing.h"

#include <FluentQt/BasicInput.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/Layout.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>

namespace fluent_b = fluent::basicinput;
namespace fluent_tf = fluent::textfields;

FaceEnrollDialog::FaceEnrollDialog(int uid, QWidget* parent)
    : QDialog(parent), m_uid(uid) {
    m_viewModel = new FaceEnrollViewModel(this);

    setWindowTitle(QStringLiteral("录入面部数据"));
    setFixedSize(400, 520);
    setModal(true);

    setupUi();
    bindViewModel();
}

FaceEnrollDialog::~FaceEnrollDialog() {
    if (m_viewModel) {
        m_viewModel->stopEnroll();
    }
}

void FaceEnrollDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(Spacing::Large, Spacing::Large, Spacing::Large, Spacing::Large);
    mainLayout->setSpacing(Spacing::Standard);
    mainLayout->setAlignment(Qt::AlignCenter);

    m_titleLabel = new fluent_tf::Label(QStringLiteral("面部特征录入"), this);
    m_titleLabel->setFluentTypography(Typography::FontRole::Title);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    m_tipLabel = new fluent_tf::Label(QStringLiteral("请保持面部位于圆框中央，光线充足且无遮挡"), this);
    m_tipLabel->setFluentTypography(Typography::FontRole::Caption);
    m_tipLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_tipLabel);

    auto* card = new fluent::layout::Card(this);
    card->setAppearance(fluent::layout::Card::Layer);
    card->setBorderVisible(true);
    card->setFixedSize(320, 320);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setAlignment(Qt::AlignCenter);

    m_scannerWidget = new FaceScannerWidget(card);
    cardLayout->addWidget(m_scannerWidget, 0, Qt::AlignCenter);

    mainLayout->addWidget(card, 0, Qt::AlignCenter);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(Spacing::Standard);

    m_cancelBtn = new fluent_b::Button(QStringLiteral("取消"), this);
    connect(m_cancelBtn, &fluent_b::Button::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    m_actionBtn = new fluent_b::Button(QStringLiteral("重试"), this);
    m_actionBtn->setFluentStyle(fluent_b::Button::Accent);
    m_actionBtn->hide();
    connect(m_actionBtn, &fluent_b::Button::clicked, this, [this]() {
        m_actionBtn->hide();
        m_viewModel->startEnroll(m_uid);
    });
    btnLayout->addWidget(m_actionBtn);

    mainLayout->addLayout(btnLayout);
}

void FaceEnrollDialog::bindViewModel() {
    m_viewModel->observe(this, &FaceEnrollDialog::renderState);

    connect(m_viewModel, &FaceEnrollViewModel::frameReceived, m_scannerWidget,
            &FaceScannerWidget::setFrame);

    connect(m_viewModel, &FaceEnrollViewModel::enrollSuccess, this, [this]() {
        QTimer::singleShot(1500, this, &QDialog::accept);
    });
}

void FaceEnrollDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (m_viewModel && m_uid > 0) {
        m_viewModel->startEnroll(m_uid);
    }
}

void FaceEnrollDialog::closeEvent(QCloseEvent* event) {
    if (m_viewModel) {
        m_viewModel->stopEnroll();
    }
    QDialog::closeEvent(event);
}

void FaceEnrollDialog::renderState(const FaceEnrollState& state) {
    if (state.isEnrolling || state.enrollSuccess) {
        m_scannerWidget->setScanState(state.scanState, state.message);
        m_actionBtn->hide();
    } else {
        m_scannerWidget->setScanState(state.scanState, state.message);
        if (state.scanState == FaceScannerWidget::ScanState::Error && m_uid > 0) {
            m_actionBtn->show();
        } else {
            m_actionBtn->hide();
        }
    }
}
