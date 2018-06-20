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

namespace {

const char kGplLink[] = "https://www.gnu.org/licenses/gpl.html";
const char kGplTranslationLink[] = "https://www.gnu.org/licenses/translations.html";
const char kHomeLink[] = "https://aspia.org";
const char kGitHubLink[] = "https://github.com/dchapyshev/aspia";

const char* kDevelopers[] = { "Dmitry Chapyshev (dmitry@aspia.ru)" };

const char* kTranslators[] =
{
    "Dmitry Chapyshev (Russian)",
    "Mark Jansen (Dutch)"
};

const char* kThirdParty[] =
{
    "Qt Framework &copy; 2015 The Qt Company Ltd., GNU General Public License 3.0",
    "libvpx &copy; 2010, The WebM Project authors, BSD 3-Clause License",
    "libyuv &copy; 2011 The LibYuv Project Authors, BSD 3-Clause License",
    "libsodium &copy; 2013-2017 Frank Denis, ISC License",
    "protobuf &copy; 2014 Google Inc., BSD 3-Clause License",
    "zlib-ng &copy; 1995-2013 Jean-loup Gailly and Mark Adler, Zlib License",
    "FatCow Icons &copy; 2009-2014 FatCow Web Hosting, Creative Commons Attribution 3.0 License",
    "Fugue Icons &copy; 2013 Yusuke Kamiyamane, Creative Commons Attribution 3.0 License"
};

QString createList(const QString& title, const char* array[], size_t array_size)
{
    if (!array_size)
        return QString();

    QString list;

    for (size_t i = 0; i < array_size; ++i)
    {
        list.append(QString("&bull; %1").arg(array[i]));
        if (i + 1 != array_size)
            list.append(QLatin1String("<br/>"));
    }

    return QString("<b>%1</b><br>%2").arg(title).arg(list);
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    ui.setupUi(this);

    ui.label_version->setText(tr("Version: %1").arg(QLatin1String(ASPIA_VERSION_STRING)));

    QString license =
        QString("%1<br>%2<br><a href='%3'>%3</a>")
        .arg(tr("Aspia is free software released under GNU General Public License 3."))
        .arg(tr("You can get a copy of license here:"))
        .arg(kGplLink);

    QString license_translation =
        QString("%1<br><a href='%2'>%2</a>")
        .arg(tr("You can also get a translation of GNU GPL license here:"))
        .arg(kGplTranslationLink);

    QString links =
        QString("<b>%1</b><br>%2 <a href='%3'>%3</a><br>%4 <a href='%5'>%5</a>")
        .arg(tr("Links:"))
        .arg(tr("Home page:")).arg(kHomeLink)
        .arg(tr("GitHub page:")).arg(kGitHubLink);

    QString developers =
        createList(tr("Developers:"), kDevelopers, _countof(kDevelopers));
    QString translators =
        createList(tr("Translators:"), kTranslators, _countof(kTranslators));
    QString third_party =
        createList(tr("Third-party components:"), kThirdParty, _countof(kThirdParty));

    QString html;

    html += "<html><body>";
    html += "<p>" + license + "</p>";
    html += "<p>" + license_translation + "</p>";
    html += "<p>" + links + "</p>";
    html += "<p>" + developers + "</p>";
    html += "<p>" + translators + "</p>";
    html += "<p>" + third_party + "</p>";
    html += "</body><html>";

    ui.text_edit->setHtml(html);

    connect(ui.push_button_donate, &QPushButton::pressed, [this]()
    {
        QDesktopServices::openUrl(QUrl(tr("https://aspia.org/en/donate.html")));
    });

    connect(ui.push_button_close, &QPushButton::pressed, this, &AboutDialog::close);
}

} // namespace aspia
