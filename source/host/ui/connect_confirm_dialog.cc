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

#include "host/ui/connect_confirm_dialog.h"

#include <QAbstractButton>
#include <QPushButton>
#include <QScreen>
#include <QTimer>

#include "base/logging.h"
#include "common/desktop/session_type.h"
#include "proto/user.h"
#include "ui_connect_confirm_dialog.h"

//--------------------------------------------------------------------------------------------------
ConnectConfirmDialog::ConnectConfirmDialog(
    const proto::user::ConfirmationRequest& request, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::ConnectConfirmDialog>()),
      timer_(new QTimer(this)),
      auto_accept_(request.timeout() > 0)
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);
    setWindowFlag(Qt::WindowStaysOnTopHint);

    connect(timer_, &QTimer::timeout, this, &ConnectConfirmDialog::onTimeout);
    connect(ui->button_box, &QDialogButtonBox::clicked,
            this, &ConnectConfirmDialog::onButtonBoxClicked);

    if (auto_accept_)
    {
        time_left_ = DurationCast<Seconds>(Milliseconds(request.timeout()));
    }
    else
    {
        time_left_ = Seconds(60);
    }

    QString user_name =
        "<b>" + QString::fromStdString(request.user_name()).toHtmlEscaped() + "</b>";
    QString computer_name =
        "<b>" + QString::fromStdString(request.computer_name()).toHtmlEscaped() + "</b>";
    QString session_name = "<b>" + sessionName(request.session_type()) + "</b>";

    message_ = tr("User %1 (%2) is trying to connect to your computer with %3 session type.")
        .arg(user_name, computer_name, session_name);
    question_ = tr("Allow the connection?");

    updateMessage();

    timer_->start(Seconds(1));

    QTimer::singleShot(Milliseconds(0), this, [this]()
    {
        QRect screen_rect = QApplication::primaryScreen()->geometry();

        int x = (screen_rect.width() / 2) - (width() / 2);
        int y = (screen_rect.height() / 2) - (height() / 2);

        move(x, y);
    });
}

//--------------------------------------------------------------------------------------------------
ConnectConfirmDialog::~ConnectConfirmDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ConnectConfirmDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->button_box->standardButton(button);
    if (standard_button == QDialogButtonBox::Yes)
    {
        LOG(INFO) << "[ACTION] 'Yes' button clicked";
        accept();
    }
    else
    {
        LOG(INFO) << "[ACTION] 'No' button clicked";
        reject();
    }

    close();
}

//--------------------------------------------------------------------------------------------------
void ConnectConfirmDialog::onTimeout()
{
    time_left_ -= Seconds(1);

    if (time_left_ <= Seconds::zero())
    {
        if (auto_accept_)
        {
            LOG(INFO) << "Accept connection by timeout";
            accept();
        }
        else
        {
            LOG(INFO) << "Reject connection by timeout";
            reject();
        }

        close();
    }
    else
    {
        updateMessage();
    }
}

//--------------------------------------------------------------------------------------------------
void ConnectConfirmDialog::updateMessage()
{
    QString timeout_string;

    if (auto_accept_)
    {
        timeout_string =
            tr("The connection will be automatically accepted after %n seconds.", "", static_cast<int>(time_left_.count()));
    }
    else
    {
        timeout_string =
            tr("The connection will be automatically rejected after %n seconds.", "", static_cast<int>(time_left_.count()));
    }

    ui->label_msg->setText(message_ + "<br/>" + timeout_string + "<br/><b>" + question_ + "</b>");
}
