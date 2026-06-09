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

#include "client/ui/hosts/router_status_widget.h"

#include <QEvent>
#include <QPlainTextEdit>

#include "base/logging.h"
#include "ui_router_status_widget.h"

//--------------------------------------------------------------------------------------------------
RouterStatusWidget::RouterStatusWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER, parent),
      ui(std::make_unique<Ui::RouterStatusWidget>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);
}

//--------------------------------------------------------------------------------------------------
RouterStatusWidget::~RouterStatusWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::showRouter(qint64 router_id, const QStringList& history)
{
    router_id_ = router_id;

    ui->edit_status->clear();
    for (const QString& line : history)
        ui->edit_status->appendPlainText(line);
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterStatusWidget::saveState()
{
    return QByteArray();
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::restoreState(const QByteArray& /* state */)
{
    // Nothing to restore.
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::onLogMessage(qint64 router_id, const QString& message)
{
    if (router_id != router_id_)
        return;

    ui->edit_status->appendPlainText(message);
}

//--------------------------------------------------------------------------------------------------
void RouterStatusWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    ContentWidget::changeEvent(event);
}
