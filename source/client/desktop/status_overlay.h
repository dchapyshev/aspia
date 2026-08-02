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

#ifndef CLIENT_DESKTOP_STATUS_OVERLAY_H
#define CLIENT_DESKTOP_STATUS_OVERLAY_H

#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;

class StatusOverlay final : public QWidget
{
    Q_OBJECT

public:
    explicit StatusOverlay(QWidget* parent);
    ~StatusOverlay() final;

    // Shows the overlay with an in-progress message (busy indicator visible) or a final error
    // message (busy indicator hidden). Hide the overlay with hide() once the session is up.
    void setProgress(const QString& message);
    void setError(const QString& message);

signals:
    // Emitted when the user clicks the close button to cancel the connection or dismiss an error.
    void sig_closeRequested();

protected:
    // QObject implementation.
    bool eventFilter(QObject* object, QEvent* event) final;

private:
    void showMessage(const QString& message, bool busy);

    QLabel* message_label_ = nullptr;
    QProgressBar* busy_bar_ = nullptr;
    QPushButton* close_button_ = nullptr;

    Q_DISABLE_COPY_MOVE(StatusOverlay)
};

#endif // CLIENT_DESKTOP_STATUS_OVERLAY_H
