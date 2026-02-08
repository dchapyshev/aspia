//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "common/ui/about_dialog.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSysInfo>
#include <QTextStream>

#include "build/version.h"
#include "base/logging.h"
#include "ui_about_dialog.h"

#include <asio/version.hpp>
#include <curl/curl.h>
#include <google/protobuf/stubs/common.h>
#include <libyuv.h>
#include <openssl/crypto.h>
#include <opus_defines.h>
#include <sqlite3.h>
#include <vpx/vpx_codec.h>
#include <zstd.h>

namespace common {

namespace {

const char kGplLink[] = "https://www.gnu.org/licenses/gpl.html";
const char kGplTranslationLink[] = "https://www.gnu.org/licenses/translations.html";
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
    "libvpx &copy; 2010, The WebM Project authors; BSD 3-Clause License",
    "libyuv &copy; 2011 The LibYuv Project Authors; BSD 3-Clause License",
    "openssl &copy; 1998-2018 The OpenSSL Project; OpenSSL License",
    "opus &copy; 2001-2011 Xiph.Org, Skype Limited, Octasic, Jean-Marc Valin, Timothy B. Terriberry,"
        " CSIRO, Gregory Maxwell, Mark Borgerding, Erik de Castro Lopo; BSD License",
    "protobuf &copy; 2014 Google Inc.; BSD 3-Clause License",
    "qt &copy; 2015 The Qt Company Ltd.; GNU General Public License 3.0",
    "zstd &copy; 2016 Yann Collet, Facebook, Inc.; BSD License"
};

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
            list.append(QLatin1String("<br/>"));
    }

    return QString("<b>%1</b><br>%2").arg(title, list);
}

} // namespace

//--------------------------------------------------------------------------------------------------
AboutDialog::AboutDialog(const QString& application_name, QWidget* parent)
    : QDialog(parent),
      ui(new Ui::AboutDialog())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->label_name->setText(application_name);
    ui->label_version->setText(tr("Version: %1 (%2)")
        .arg(ASPIA_VERSION_STRING, QSysInfo::buildCpuArchitecture()));

    QString license =
        QString("%1<br>%2<br><a href='%3'>%3</a>")
        .arg(tr("SmartCafe is free software released under GNU General Public License 3."),
             tr("You can get a copy of license here:"),
             kGplLink);

    QString license_translation =
        QString("%1<br><a href='%2'>%2</a>")
        .arg(tr("You can also get a translation of GNU GPL license here:"), kGplTranslationLink);

    QString links =
        QString("<b>%1</b><br>%2 <a href='%3'>%3</a><br>%4 <a href='%5'>%5</a>")
        .arg(tr("Links:"),
             tr("Home page:"), kHomeLink,
             tr("GitHub page:"), kGitHubLink);

    QString developers =
        createList(tr("Developers:"), kDevelopers, std::size(kDevelopers));
    QString translators =
        createList(tr("Translators:"), kTranslators, std::size(kTranslators));
    QString third_party =
        createList(tr("Third-party components:"), kThirdParty, std::size(kThirdParty));
    QString icons =
        "<b>" + tr("Graphics and images:") + "</b><br>&bull; " +
        tr("Icons by %1").arg("<a href='https://icons8.com'>Icons8</a>");

    QString html;

    html += "<html><body>";
    html += "<p>" + license + "</p>";
    html += "<p>" + license_translation + "</p>";
    html += "<p>" + links + "</p>";
    html += "<p>" + developers + "</p>";
    html += "<p>" + translators + "</p>";
    html += "<p>" + third_party + "</p>";
    html += "<p>" + icons + "</p>";
    html += "</body><html>";

    ui->text_edit->setHtml(html);

    QListWidget* list = ui->list_service;

    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QListWidget::customContextMenuRequested,
            this, &AboutDialog::onServiceContextMenu);

    list->addItem(tr("Path: %1").arg(QApplication::applicationFilePath()));
    list->addItem(tr("Compilation date: %1").arg(__DATE__));
    list->addItem(tr("Compilation time: %1").arg(__TIME__));

#if defined(GIT_CURRENT_BRANCH) && defined(GIT_COMMIT_HASH)
    list->addItem(tr("Git branch: %1").arg(GIT_CURRENT_BRANCH));
    list->addItem(tr("Git commit: %1").arg(GIT_COMMIT_HASH));
#endif

    list->addItem(tr("Logging directory: %1").arg(base::loggingDirectory()));
    list->addItem(tr("Logging file: %1").arg(base::loggingFile()));

    auto add_version = [list](const char* name, const QString& version)
    {
        list->addItem(tr("%1 version: %2").arg(name, version));
    };

    add_version("asio", QString("%1.%2.%3")
        .arg(ASIO_VERSION / 100000).arg(ASIO_VERSION / 100 % 1000).arg(ASIO_VERSION % 100));
    add_version("curl", curl_version());

    add_version("libvpx", vpx_codec_version_str());
    add_version("libyuv", QString::number(LIBYUV_VERSION));
    add_version("openssl", OpenSSL_version(OPENSSL_VERSION));
    add_version("opus", opus_get_version_string());

    QString protobuf_version =
        QString::fromStdString(google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION));
    add_version("protobuf", protobuf_version);

    add_version("qt", qVersion());
    add_version("sqlite", SQLITE_VERSION);
    add_version("zstd", ZSTD_versionString());

    connect(ui->push_button_donate, &QPushButton::clicked, this, []()
    {
        LOG(INFO) << "[ACTION] Donate button clicked";
        QDesktopServices::openUrl(QUrl("https://aspia.org/donate"));
    });

    connect(ui->push_button_close, &QPushButton::clicked, this, [this]()
    {
        LOG(INFO) << "[ACTION] Close button clicked";
        close();
    });
}

//--------------------------------------------------------------------------------------------------
AboutDialog::~AboutDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void AboutDialog::onServiceContextMenu(const QPoint& pos)
{
    LOG(INFO) << "[ACTION] Service context menu";

    QMenu menu;

    QAction* save_action = menu.addAction(tr("Save to file..."));

    if (menu.exec(ui->list_service->viewport()->mapToGlobal(pos)) == save_action)
    {
        LOG(INFO) << "[ACTION] Save action";

        QString selected_filter;
        QString file_path = QFileDialog::getSaveFileName(this,
                                                         tr("Save File"),
                                                         QString(),
                                                         tr("TXT files (*.txt)"),
                                                         &selected_filter);
        if (file_path.isEmpty() || selected_filter.isEmpty())
        {
            LOG(INFO) << "File path not selected";
            return;
        }

        LOG(INFO) << "Selected file path:" << file_path.toStdString();

        QFile file(file_path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            LOG(ERROR) << "Unable to open file:" << file.errorString().toStdString();
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Could not open file for writing."),
                                 QMessageBox::Ok);
            return;
        }

        QListWidget* list = ui->list_service;
        QTextStream stream(&file);

        for (int i = 0; i < list->count(); ++i)
        {
            QListWidgetItem* item = list->item(i);
            stream << item->text() << Qt::endl;
        }
    }
}

} // namespace common
