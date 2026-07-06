//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "common/android/about_widget.h"

#include <QLabel>
#include <QSysInfo>
#include <QVBoxLayout>

#include "build/version.h"
#include "common/android/controls.h"

namespace {

const char kGplLink[] = "https://www.gnu.org/licenses/gpl.html";
const char kHomeLink[] = "https://aspia.org";
const char kGitHubLink[] = "https://github.com/dchapyshev/aspia";

const char* kDevelopers[] = { "Dmitry Chapyshev (dmitry@aspia.ru)" };

const char* kTranslators[] =
{
    "Dmitry Chapyshev (Russian)",
    "Felipe Borela (Portuguese Brazilian)",
    "Giuseppe Bogetti (Italian)",
    "Gregor Doroschenko (German)",
    "Lyhyrda Myhaylo (Ukrainian)",
    "Mark Jansen (Dutch)",
    "Shun-An Lee (Chinese (Taiwan))",
    "Wang Qiang (Chinese (China))"
};

const char* kThirdParty[] =
{
    "asio &copy; 2003-2018 Christopher M. Kohlhoff; Boost Software License 1.0",
    "curl &copy; 1996-2022 Daniel Stenberg, and many contributors; CURL License",
    "enet &copy; 2002-2024 Lee Salzman; MIT License",
    "libvpx &copy; 2010, The WebM Project authors; BSD 3-Clause License",
    "libyuv &copy; 2011 The LibYuv Project Authors; BSD 3-Clause License",
    "openssl &copy; 1998-2018 The OpenSSL Project; OpenSSL License",
    "opus &copy; 2001-2011 Xiph.Org, Skype Limited, Octasic, Jean-Marc Valin, Timothy B. Terriberry,"
        " CSIRO, Gregory Maxwell, Mark Borgerding, Erik de Castro Lopo; BSD License",
    "protobuf &copy; 2014 Google Inc.; BSD 3-Clause License",
    "qt &copy; 2015 The Qt Company Ltd.; GNU General Public License 3.0",
    "zstd &copy; 2016 Yann Collet, Facebook, Inc.; BSD License"
};

constexpr int kContentMargin = 16;

//--------------------------------------------------------------------------------------------------
QString createList(const QString& title, const char* array[], size_t array_size)
{
    if (!array_size)
        return QString();

    QString list;

    for (size_t i = 0; i < array_size; ++i)
    {
        list.append(QString("&bull; %1").arg(array[i]));
        if (i + 1 != array_size)
            list.append("<br/>");
    }

    return QString("<b>%1</b><br>%2").arg(title, list);
}

} // namespace

//--------------------------------------------------------------------------------------------------
AboutWidget::AboutWidget(QWidget* parent)
    : ScrollArea(parent)
{
    buildContent();
}

//--------------------------------------------------------------------------------------------------
AboutWidget::~AboutWidget() = default;

//--------------------------------------------------------------------------------------------------
void AboutWidget::buildContent()
{
    QString version = QString("<p><b>Aspia</b><br>%1</p>").arg(
        tr("Version: %1 (%2)").arg(ASPIA_VERSION_STRING, QSysInfo::buildCpuArchitecture()));

    QString license = QString("<p>%1<br>%2<br><a href='%3'>%3</a></p>")
        .arg(tr("Aspia is free software released under GNU General Public License 3."),
             tr("You can get a copy of license here:"), kGplLink);

    QString links = QString("<p><b>%1</b><br>%2 <a href='%3'>%3</a><br>%4 <a href='%5'>%5</a></p>")
        .arg(tr("Links"), tr("Home page:"), kHomeLink, tr("GitHub page:"), kGitHubLink);

    QString html;
    html += "<html><body>";
    html += version;
    html += license;
    html += links;
    html += "<p>" + createList(tr("Developers"), kDevelopers, std::size(kDevelopers)) + "</p>";
    html += "<p>" + createList(tr("Translators"), kTranslators, std::size(kTranslators)) + "</p>";
    html += "<p>" + createList(tr("Third-party components"), kThirdParty, std::size(kThirdParty)) + "</p>";
    html += "</body></html>";

    QLabel* text = new QLabel();
    text->setFont(Controls::scaledFont(text->font(), Controls::kFontScale));
    text->setTextFormat(Qt::RichText);
    text->setWordWrap(true);
    text->setOpenExternalLinks(true);
    text->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    text->setText(html);

    QWidget* content = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kContentMargin, kContentMargin, kContentMargin, kContentMargin);
    layout->addWidget(text);
    layout->addStretch();

    // setWidget() deletes the previously set content widget.
    setWidget(content);
}
