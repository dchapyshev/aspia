#include <QApplication>
#include <QWidget>
#include <QPointer>
#include <QScreen>

#include "base/win/mini_dump_writer.h"

int main(int argc, char** argv)
{
    base::installFailureHandler(L"test");

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);

    QPointer<QWidget> widget = new QWidget();
    QList<QScreen*> screens = app.screens();

    for (const auto& screen : std::as_const(screens))
    {
        QObject::connect(screen, &QScreen::geometryChanged, [&]()
        {
            delete widget;
            widget = new QWidget();
            widget->showNormal();
            widget->activateWindow();
        });
    }

    widget->showNormal();
    widget->activateWindow();

    return app.exec();
}
