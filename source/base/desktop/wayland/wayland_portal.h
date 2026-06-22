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

#ifndef BASE_DESKTOP_WAYLAND_WAYLAND_PORTAL_H
#define BASE_DESKTOP_WAYLAND_WAYLAND_PORTAL_H

#include <QObject>
#include <QPoint>
#include <QSize>

#include <cstdint>

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
        uint32_t node_id = 0;
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
    void notifyPointerButton(int32_t button, bool pressed);
    void notifyPointerAxis(double dx, double dy);
    void notifyPointerAxisDiscrete(uint32_t axis, int32_t steps);
    void notifyKeyboardKeycode(int32_t keycode, bool pressed);
    void notifyKeyboardKeysym(int32_t keysym, bool pressed);

signals:
    void sig_started(bool success);
    void sig_closed();

private slots:
    // Single handler for org.freedesktop.portal.Request::Response. The pending step determines how
    // the results are interpreted (requests are issued strictly sequentially).
    void onResponse(uint response, const QVariantMap& results);

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

    void createSession();
    void selectDevices();
    void selectSources();
    void startSession();
    void openPipeWireRemote();
    void fail();

    QDBusInterface* screen_cast_ = nullptr;
    QDBusInterface* remote_desktop_ = nullptr;

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

#endif // BASE_DESKTOP_WAYLAND_WAYLAND_PORTAL_H
