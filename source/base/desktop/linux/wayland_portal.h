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

#ifndef BASE_DESKTOP_LINUX_WAYLAND_PORTAL_H
#define BASE_DESKTOP_LINUX_WAYLAND_PORTAL_H

#include <QObject>
#include <QPoint>
#include <QSize>

class QDBusInterface;

// Negotiates a single xdg-desktop-portal session that provides both screen capture (ScreenCast) and
// input injection (RemoteDesktop). One session means the user is asked for permission only once and
// the same PipeWire stream is shared between capture and input.
//
// The negotiation is asynchronous (it involves a user permission dialog); sig_started is emitted
// with the result. On success pipeWireFd() and stream() describe the capture source, and the
// notify*() methods inject input on the same session.
class WaylandPortal final : public QObject
{
    Q_OBJECT

public:
    explicit WaylandPortal(QObject* parent = nullptr);
    ~WaylandPortal() final;

    struct Stream
    {
        quint32 node_id = 0;
        QPoint position;
        QSize size;
    };

    // Starts the session. |restore_token| (from a previous restoreToken()) lets the portal skip the
    // permission dialog if the user allowed persistence.
    void start(const QString& restore_token = QString());

    bool isStarted() const;

    // File descriptor for pw_context_connect_fd(). Owned by this object; valid while started.
    int pipeWireFd() const;
    const Stream& stream() const;

    // Token for persisting the grant across sessions; valid after a successful start.
    QString restoreToken() const;

    // Remote desktop input. Pointer coordinates are absolute within the captured stream. Button and
    // key codes are Linux evdev codes (X keycode minus 8 for keys).
    void notifyPointerMotionAbsolute(double x, double y);
    void notifyPointerButton(qint32 button, bool pressed);
    void notifyPointerAxis(double dx, double dy);
    void notifyPointerAxisDiscrete(quint32 axis, qint32 steps);
    void notifyKeyboardKeycode(qint32 keycode, bool pressed);
    void notifyKeyboardKeysym(qint32 keysym, bool pressed);

signals:
    void sig_started(bool success);
    void sig_closed();

private slots:
    // Single handler for org.freedesktop.portal.Request::Response. The pending step determines how
    // the results are interpreted (requests are issued strictly sequentially).
    void onResponse(uint response, const QVariantMap& results);

    // org.freedesktop.portal.Session::Closed: the compositor or user ended the session.
    void onSessionClosed(const QVariantMap& details);

private:
    enum class Step
    {
        IDLE,
        CREATE_SESSION,
        SELECT_DEVICES,
        SELECT_SOURCES,
        START,
        FAILED,
        STARTED
    };

    QString newToken(const QString& prefix);
    // Connects onResponse to the predicted request handle and returns the handle_token to put in the
    // call options.
    QString prepareRequest(const QString& token_prefix);
    // Issues an asynchronous portal request; the result arrives via onResponse, this only reports a
    // failure of the call itself. Async (not QDBus::Block) so the delayed Start response - which the
    // compositor sends only after the user confirms the dialog - is delivered through the event loop.
    void issueRequest(QDBusInterface* interface, const QString& method, const QVariantList& arguments,
                      const char* description);

    void queryCapabilities();
    void createSession();
    void selectDevices();
    void selectSources();
    void startSession();
    void openPipeWireRemote();
    void fail();

    // Reads a uint portal property (version/AvailableCursorModes/...) via the Properties interface.
    quint32 portalProperty(const QString& interface, const QString& property) const;

    QDBusInterface* screen_cast_ = nullptr;
    QDBusInterface* remote_desktop_ = nullptr;

    // Portal capabilities, queried before negotiation so options are gated on what is supported.
    quint32 screen_cast_version_ = 0;
    quint32 remote_desktop_version_ = 0;
    quint32 available_cursor_modes_ = 0;
    quint32 available_source_types_ = 0;
    quint32 available_device_types_ = 0;

    QString sender_name_; // Unique bus name with the leading ':' removed and '.' -> '_'.
    QString session_handle_;
    QString restore_token_in_;
    QString restore_token_;
    QString pending_request_path_;
    Step step_ = Step::IDLE;
    uint token_counter_ = 0;

    int pipewire_fd_ = -1;
    Stream stream_;

    Q_DISABLE_COPY_MOVE(WaylandPortal)
};

#endif // BASE_DESKTOP_LINUX_WAYLAND_PORTAL_H
