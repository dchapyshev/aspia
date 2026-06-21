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

#ifndef CLIENT_ANDROID_CHAT_VIEW_H
#define CLIENT_ANDROID_CHAT_VIEW_H

#include <QList>
#include <QWidget>

class IconButton;
class Label;
class QTextEdit;
class QVBoxLayout;
class QScrollArea;

class ChatView final : public QWidget
{
    Q_OBJECT

public:
    explicit ChatView(QWidget* parent = nullptr);
    ~ChatView() final;

    void addMessage(const QString& source, const QString& text, bool outgoing, qint64 timestamp);
    void addStatus(const QString& text);
    void clear();

    // The always-visible line above the input field; an empty text keeps its reserved height.
    void setStatusText(const QString& text);

    // Enables the input field and send button (disabled until the session connects).
    void setInputEnabled(bool enabled);

signals:
    void sig_sendText(const QString& text);

    // Emitted while the user is typing (each text change with non-empty content).
    void sig_typing();

protected:
    // QWidget implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;
    void resizeEvent(QResizeEvent* event) final;

private slots:
    void onSend();
    void updateInputHeight();

private:
    void appendRow(QWidget* row);

    QVBoxLayout* messages_layout_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    Label* status_ = nullptr;
    QTextEdit* input_ = nullptr;
    IconButton* send_button_ = nullptr;

    // Message bubbles, kept so their maximum width can be re-clamped on resize.
    QList<QWidget*> bubbles_;

    // Set when a row is appended while the list is at the bottom, so the next range change (after the
    // new row is laid out) scrolls to follow it.
    bool autoscroll_ = false;

    Q_DISABLE_COPY_MOVE(ChatView)
};

#endif // CLIENT_ANDROID_CHAT_VIEW_H
