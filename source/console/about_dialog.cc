//
// PROJECT:         Aspia
// FILE:            console/about_dialog.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/about_dialog.h"

#include <QAbstractButton>
#include <QDesktopServices>

#include "version.h"

namespace aspia {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    ui.label_version->setText(tr("Version: %1").arg(QLatin1String(ASPIA_VERSION_STRING)));

    connect(ui.push_button_donate, &QPushButton::pressed, [this]()
    {
        QDesktopServices::openUrl(QUrl(tr("https://aspia.net/donate")));
    });

    connect(ui.push_button_close, &QPushButton::pressed, [this]()
    {
        close();
    });
}

AboutDialog::~AboutDialog() = default;

} // namespace aspia
