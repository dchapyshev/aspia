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

#include "client/desktop/management/router_status_widget.h"

#include <QDataStream>
#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTreeWidget>

#include "base/gui_application.h"
#include "base/logging.h"
#include "client/router_controller.h"
#include "ui_router_status_widget.h"

namespace {

enum Column
{
    COLUMN_TIME = 0,
    COLUMN_EVENT
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterStatusWidget::RouterStatusWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER, parent),
      ui(std::make_unique<Ui::RouterStatusWidget>()),
      status_events_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->label_two_factor_icon->setPixmap(GuiApplication::svgPixmap(":/img/lock.svg", QSize(24, 24)));
    ui->frame_two_factor->setVisible(false);

    connect(ui->button_two_factor, &QPushButton::clicked,
            this, &RouterStatusWidget::onTwoFactorClicked);
}

//--------------------------------------------------------------------------------------------------
RouterStatusWidget::~RouterStatusWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::showRouter(qint64 router_id, const QList<RouterEvent>& events)
{
    router_id_ = router_id;

    ui->tree_events->clear();
    for (const RouterEvent& event : events)
        addEvent(event);

    ui->tree_events->scrollToBottom();
    updateStatusLabel();
    updateTwoFactorPrompt();
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterStatusWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_events->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
    {
        ui->tree_events->header()->restoreState(columns_state);
        ui->tree_events->header()->setSectionsClickable(true);
        ui->tree_events->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabel();

    statusbar->addWidget(status_events_label_);
    status_events_label_->show();
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_events_label_);
    status_events_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::onEvent(qint64 router_id, const RouterEvent& event)
{
    if (router_id != router_id_)
        return;

    addEvent(event);
    ui->tree_events->scrollToBottom();
    updateStatusLabel();
    updateTwoFactorPrompt();
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::onStatusChanged(qint64 router_id)
{
    if (router_id != router_id_)
        return;

    updateTwoFactorPrompt();
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::onTwoFactorClicked()
{
    emit sig_twoFactorClicked(router_id_);
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::addEvent(const RouterEvent& event)
{
    QString icon_path;
    switch (event.severity)
    {
        case RouterEvent::Severity::WARNING:  icon_path = ":/img/box-important.svg";   break;
        case RouterEvent::Severity::CRITICAL: icon_path = ":/img/high-importance.svg"; break;
        case RouterEvent::Severity::INFO:     icon_path = ":/img/info.svg";            break;
    }

    QTreeWidgetItem* item = new QTreeWidgetItem(ui->tree_events);
    item->setText(COLUMN_TIME, event.time.toString("yyyy-MM-dd HH:mm:ss"));
    item->setText(COLUMN_EVENT, event.text);
    item->setIcon(COLUMN_TIME, QIcon(icon_path));

    while (ui->tree_events->topLevelItemCount() > RouterController::kMaxStoredEvents)
        delete ui->tree_events->takeTopLevelItem(0);
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::updateStatusLabel()
{
    status_events_label_->setText(tr("%n event(s)", "", ui->tree_events->topLevelItemCount()));
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::updateTwoFactorPrompt()
{
    TwoFactorPrompt* prompt = RouterController::twoFactorPrompt(router_id_);
    if (!prompt)
    {
        ui->frame_two_factor->setVisible(false);
        return;
    }

    // A blocked account has nothing to enter. The router does not look at codes while the block
    // runs, so the button goes away with the prompt.
    if (prompt->blockedSeconds() > 0)
    {
        ui->label_two_factor->setText(
            tr("Two-factor authentication is blocked after too many failed attempts."));
        ui->button_two_factor->setVisible(false);
        ui->frame_two_factor->setVisible(true);
        return;
    }

    ui->button_two_factor->setVisible(true);

    // An account with no secret yet is walked through the enrollment first: the operator scans the
    // code the router handed out and only then types what their application shows.
    if (prompt->otpauthUri().isEmpty())
    {
        ui->label_two_factor->setText(tr("The router is waiting for a two-factor code."));
        ui->button_two_factor->setText(tr("Enter Code"));
    }
    else
    {
        ui->label_two_factor->setText(tr("Two-factor authentication has to be set up for this router."));
        ui->button_two_factor->setText(tr("Set Up"));
    }

    ui->frame_two_factor->setVisible(true);
}
