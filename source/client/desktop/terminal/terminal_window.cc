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

#include "client/desktop/terminal/terminal_window.h"

#include <QAction>
#include <QVBoxLayout>

#include "base/logging.h"
#include "client/client_terminal.h"
#include "client/desktop/terminal/terminal_widget.h"
#include "common/desktop/session_type.h"
#include "proto/peer.h"

//--------------------------------------------------------------------------------------------------
TerminalWindow::TerminalWindow(QWidget* parent)
    : ClientWindow(proto::peer::SESSION_TYPE_TERMINAL, parent),
      terminal_widget_(new TerminalWidget(this))
{
    LOG(INFO) << "Ctor";

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(terminal_widget_);

    setFocusProxy(terminal_widget_);

    auto make_action = [this](proto::peer::SessionType type)
    {
        QAction* action = new QAction(sessionIcon(type), sessionName(type), this);
        connect(action, &QAction::triggered, this, [this, type]()
        {
            emit sig_connectRequested(sessionState()->host(), type);
        });
        return action;
    };

    action_desktop_ = make_action(proto::peer::SESSION_TYPE_DESKTOP);
    action_file_transfer_ = make_action(proto::peer::SESSION_TYPE_FILE_TRANSFER);
    action_system_info_ = make_action(proto::peer::SESSION_TYPE_SYSTEM_INFO);
    action_text_chat_ = make_action(proto::peer::SESSION_TYPE_CHAT);
}

//--------------------------------------------------------------------------------------------------
TerminalWindow::~TerminalWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
Client* TerminalWindow::createClient()
{
    LOG(INFO) << "Create client";

    ClientTerminal* client = new ClientTerminal();

    connect(terminal_widget_, &TerminalWidget::sig_credentials,
            client, &ClientTerminal::sendCredentials, Qt::QueuedConnection);
    connect(terminal_widget_, &TerminalWidget::sig_input,
            client, &ClientTerminal::sendInput, Qt::QueuedConnection);
    connect(terminal_widget_, &TerminalWidget::sig_resize,
            client, &ClientTerminal::sendResize, Qt::QueuedConnection);

    connect(client, &ClientTerminal::sig_outputReceived,
            terminal_widget_, &TerminalWidget::writeOutput, Qt::QueuedConnection);
    connect(client, &ClientTerminal::sig_resultReceived,
            terminal_widget_, &TerminalWidget::onResult, Qt::QueuedConnection);

    return client;
}

//--------------------------------------------------------------------------------------------------
QList<QPair<Tab::ActionRole, QList<QAction*>>> TerminalWindow::tabActionGroups() const
{
    return {
        { Tab::ActionRole::ACTION,
          { action_desktop_, action_file_transfer_, action_system_info_, action_text_chat_ } }
    };
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onInternalReset()
{
    // Nothing.
}
