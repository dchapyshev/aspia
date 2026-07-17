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

#include <QVBoxLayout>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/codec/zstd_stream_decompressor.h"
#include "client/desktop/terminal/terminal_widget.h"
#include "client/workers/network_worker.h"
#include "proto/peer.h"
#include "proto/terminal.h"

namespace {

// Upper bound on the decompressed size of a single terminal-output message.
const qint64 kMaxTerminalOutputSize = 32 * 1024 * 1024; // 32 MB.

} // namespace

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

    connect(terminal_widget_, &TerminalWidget::sig_credentials, this, &TerminalWindow::onCredentials);
    connect(terminal_widget_, &TerminalWidget::sig_input, this, &TerminalWindow::onInput);
    connect(terminal_widget_, &TerminalWidget::sig_resize, this, &TerminalWindow::onResize);
}

//--------------------------------------------------------------------------------------------------
TerminalWindow::~TerminalWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onInternalReset()
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onRegisterWorkers()
{
    output_decompressor_ = std::make_unique<ZstdStreamDecompressor>();

    connect(networkWorker(), &NetworkWorker::sig_channel_0,
            this, &TerminalWindow::onChannelMessage, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onSessionStarted()
{
    // Nothing.
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onChannelMessage(const QByteArray& buffer)
{
    proto::terminal::HostToClient message;
    if (!parse(buffer, &message))
    {
        LOG(ERROR) << "Unable to parse message";
        return;
    }

    if (message.has_data())
    {
        terminal_widget_->writeOutput(
            output_decompressor_->decompress(message.data().data(), kMaxTerminalOutputSize));
    }

    if (message.has_result())
        terminal_widget_->onResult(message.result().code());
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onCredentials(const QString& user_name, const QString& password)
{
    proto::terminal::ClientToHost message;
    proto::terminal::Credentials* credentials = message.mutable_credentials();
    credentials->set_user_name(user_name.toStdString());
    credentials->set_password(password.toStdString());
    sendMessage(proto::peer::CHANNEL_ID_0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onInput(const QByteArray& data)
{
    proto::terminal::ClientToHost message;
    message.mutable_data()->set_data(std::string(data.constData(), data.size()));
    sendMessage(proto::peer::CHANNEL_ID_0, serialize(message));
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onResize(int columns, int rows)
{
    proto::terminal::ClientToHost message;
    proto::terminal::Resize* resize = message.mutable_resize();
    resize->set_columns(columns);
    resize->set_rows(rows);
    sendMessage(proto::peer::CHANNEL_ID_0, serialize(message));
}
