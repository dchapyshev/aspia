//
// Aspia Project
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

#ifndef BASE_GUI_APPLICATION_H
#define BASE_GUI_APPLICATION_H

#include <QApplication>
#include <QPalette>

#include "base/thread.h"
#include "base/translations.h"

class QLocalServer;
class QLockFile;

namespace base {
#if defined(Q_OS_WINDOWS)
class MessageWindow;
#endif // defined(Q_OS_WINDOWS)

class ScopedCryptoInitializer;
} // namespace base

namespace base {

class GuiApplication : public QApplication
{
    Q_OBJECT

public:
    GuiApplication(int& argc, char* argv[]);
    virtual ~GuiApplication() override;

    static GuiApplication* instance();
    static QThread* ioThread();

    bool isRunning();

    using Locale = Translations::Locale;
    using LocaleList = Translations::LocaleList;

    LocaleList localeList() const;
    void setLocale(const QString& locale);
    bool hasLocale(const QString& locale);

    static QByteArray svgByteArray(const QString& svg_file_path);
    static QPixmap svgPixmap(const QString& svg_file_path, const QSize& size = QSize(24, 24));
    static QIcon svgIcon(const QString& svg_file_path, const QSize& size = QSize(24, 24));
    static QImage svgImage(const QString& svg_file_path, const QSize& size);

public slots:
    void sendMessage(const QByteArray& message);

signals:
    void sig_messageReceived(const QByteArray& message);
    void sig_themeChanged();
    void sig_sessionEvent(quint32 event, quint32 session_id);
    void sig_powerEvent(quint32 event);

protected:
    bool event(QEvent* event) override;

private slots:
    void onNewConnection();

private:
    QString lock_file_name_;
    QString server_name_;

    QLockFile* lock_file_ = nullptr;
    QLocalServer* server_ = nullptr;

    base::Thread io_thread_;
    std::unique_ptr<base::ScopedCryptoInitializer> crypto_initializer_;
    std::unique_ptr<Translations> translations_;

#if defined(Q_OS_WINDOWS)
    std::unique_ptr<base::MessageWindow> message_window_;
#endif // defined(Q_OS_WINDOWS)

    Q_DISABLE_COPY(GuiApplication)
};

} // namespace base

#endif // BASE_GUI_APPLICATION_H
