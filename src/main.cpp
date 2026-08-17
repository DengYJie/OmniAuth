#include <FluentQt/FluentQt.h>

#include "core/CryptoUtils.h"
#include "data/di/AppContainer.h"
#include "domain/model/FaceTypes.h"

#include "ui/screen/main/AuthWindow.h"
#include "ui/screen/main/MainWindow.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QDebug>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QtWidgets/QApplication>

#include <thread>

int main(int argc, char* argv[]) {
    fluent::prepareHighDpiApplication();

    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QApplication a(argc, argv);
    qRegisterMetaType<AuthResult>("AuthResult");

    fluent::initializeResources();
    a.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
    fluent::StyleThemeCatalog::apply(fluent::StyleTheme::Fluent);

    if (!CryptoUtils::init()) {
        qFatal("Failed to initialize libsodium!");
    }

    AppContainer::init(false);

    MainWindow w;
    w.show();
    AuthWindow authWindow;
    authWindow.show();

    return QApplication::exec();
}
