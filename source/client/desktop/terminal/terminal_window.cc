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

#include <QImage>
#include <QKeyEvent>
#include <QPixmap>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/serialization.h"
#include "base/codec/zstd_stream_decompressor.h"
#include "client/desktop/credentials/credential_panel.h"
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
      terminal_widget_(new TerminalWidget(this)),
      credential_panel_(new CredentialPanel(this))
{
    LOG(INFO) << "Ctor";

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(terminal_widget_);

    setFocusProxy(terminal_widget_);
    terminal_widget_->installEventFilter(this);

    connect(terminal_widget_, &TerminalWidget::sig_credentials, this, &TerminalWindow::onCredentials);
    connect(terminal_widget_, &TerminalWidget::sig_input, this, &TerminalWindow::onInput);
    connect(terminal_widget_, &TerminalWidget::sig_resize, this, &TerminalWindow::onResize);

    connect(credential_panel_, &CredentialPanel::sig_credentialSelected,
            this, [this](qint64 credential_id, const QString& user_name, const SecureString& password)
    {
        setOsCredentialId(credential_id);
        terminal_widget_->login(user_name, password.toString());
    });
}

//--------------------------------------------------------------------------------------------------
TerminalWindow::~TerminalWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
bool TerminalWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == terminal_widget_ && event->type() == QEvent::KeyPress && credential_panel_->isVisible() &&
        !static_cast<QKeyEvent*>(event)->text().isEmpty())
    {
        credential_panel_->onDismiss();
    }

    return ClientWindow::eventFilter(object, event);
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
    credential_panel_->popup(osCredentialId());
}

//--------------------------------------------------------------------------------------------------
McpController::SignInResult TerminalWindow::mcpSignIn()
{
    if (!terminal_widget_->hasLoginPrompt())
        return McpController::SignInResult::NOT_REQUESTED;

    return credential_panel_->signIn() ? McpController::SignInResult::STARTED :
                                         McpController::SignInResult::NOT_SELECTED;
}

//--------------------------------------------------------------------------------------------------
bool TerminalWindow::mcpHasLoginPrompt() const
{
    return terminal_widget_->hasLoginPrompt();
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::mcpSendText(const QString& text)
{
    last_input_time_ = Clock::now();
    terminal_widget_->sendText(text);
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::mcpSendKey(int key, Qt::KeyboardModifiers modifiers)
{
    last_input_time_ = Clock::now();
    terminal_widget_->sendKey(key, modifiers);
}

//--------------------------------------------------------------------------------------------------
QSize TerminalWindow::mcpGridSize() const
{
    return terminal_widget_->gridSize();
}

//--------------------------------------------------------------------------------------------------
QPoint TerminalWindow::mcpCursorPosition() const
{
    return terminal_widget_->cursorPosition();
}

//--------------------------------------------------------------------------------------------------
QStringList TerminalWindow::mcpLines(int history) const
{
    return terminal_widget_->lines(history);
}

//--------------------------------------------------------------------------------------------------
QImage TerminalWindow::mcpScreenshot() const
{
    return terminal_widget_->grab().toImage();
}

//--------------------------------------------------------------------------------------------------
TimePoint TerminalWindow::mcpLastInputTime() const
{
    return last_input_time_;
}

//--------------------------------------------------------------------------------------------------
TimePoint TerminalWindow::mcpLastOutputTime() const
{
    return last_output_time_;
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
        last_output_time_ = Clock::now();
        terminal_widget_->writeOutput(
            output_decompressor_->decompress(message.data().data(), kMaxTerminalOutputSize));
    }

    if (message.has_result())
    {
        terminal_widget_->onResult(message.result().code());

        if (message.result().code() == proto::terminal::Result::CODE_AUTHENTICATION_ERROR)
            credential_panel_->popup(osCredentialId(), tr("Invalid user name or password."));
    }
}

//--------------------------------------------------------------------------------------------------
void TerminalWindow::onCredentials(const QString& user_name, const QString& password)
{
    credential_panel_->onDismiss();

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
