//
// PROJECT:         Aspia
// FILE:            host/host_notifier_window.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__UI__HOST_NOTIFIER_WINDOW_H
#define _ASPIA_HOST__UI__HOST_NOTIFIER_WINDOW_H

#include "host/host_notifier.h"
#include "protocol/notifier.pb.h"
#include "ui_host_notifier_window.h"

namespace aspia {

class HostNotifierWindow : public QWidget
{
    Q_OBJECT

public:
    explicit HostNotifierWindow(QWidget* parent = nullptr);
    ~HostNotifierWindow();

    void setChannelId(const QString& channel_id);

public slots:
    void sessionOpen(const proto::notifier::Session& session);
    void sessionClose(const proto::notifier::SessionClose& session_close);

signals:
    void killSession(const std::string& uuid);

protected:
    // QWidget implementation.
    bool eventFilter(QObject* object, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void quit();
    void onShowHidePressed();
    void onDisconnectAllPressed();
    void onContextMenu(const QPoint& point);

private:
    void installTranslators(const QStringList& file_list);

    Ui::HostNotifierWindow ui;

    QList<QTranslator*> translator_list_;
    QPoint start_pos_;
    QRect window_rect_;

    QPointer<HostNotifier> notifier_;
    QString channel_id_;

    Q_DISABLE_COPY(HostNotifierWindow)
};

} // namespace aspia

#endif // _ASPIA_HOST__UI__HOST_NOTIFIER_WINDOW_H
