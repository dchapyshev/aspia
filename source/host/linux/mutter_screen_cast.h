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

#ifndef HOST_LINUX_MUTTER_SCREEN_CAST_H
#define HOST_LINUX_MUTTER_SCREEN_CAST_H

#include <QDBusConnection>
#include <QObject>

#include <sys/types.h>

#include "host/linux/wayland_compositor_source.h"

class QDBusInterface;

// Captures a monitor through GNOME Mutter's private org.gnome.Mutter.ScreenCast D-Bus interface
// directly, without going through xdg-desktop-portal. The portal adds the permission dialog on top of
// this interface; calling it directly skips the dialog, which is what allows capturing the login
// screen (where no one can grant permission). The negotiation is synchronous - Mutter returns the
// session/stream object paths immediately and signals the PipeWire node a moment later.
class MutterScreenCast final : public WaylandCompositorSource
{
    Q_OBJECT

public:
    explicit MutterScreenCast(uid_t session_uid, QObject* parent = nullptr);
    ~MutterScreenCast() final;

    // Returns true if the session bus connection succeeded and org.gnome.Mutter.ScreenCast is present.
    bool isAvailable() const;

    void start() final;
    QList<MonitorInfo> monitors() const final;
    QString recordedMonitor() const final;
    void setRequestedMonitor(const QString& connector) final;

    // WaylandCaptureSource implementation.
    bool isStarted() const final;
    const Stream& stream() const final;
    int pipeWireFd() const final;
    uid_t pipeWireUid() const final;

    // WaylandInputTarget implementation.
    void notifyPointerMotionAbsolute(double x, double y) final;
    void notifyPointerButton(qint32 button, bool pressed) final;
    void notifyPointerAxis(double dx, double dy) final;
    void notifyPointerAxisDiscrete(quint32 axis, qint32 steps) final;
    void notifyKeyboardKeycode(qint32 keycode, bool pressed) final;
    void notifyKeyboardKeysym(qint32 keysym, bool pressed) final;

signals:
    void sig_closed();

private slots:
    // org.gnome.Mutter.ScreenCast.Stream::PipeWireStreamAdded - delivers the PipeWire node id.
    void onPipeWireStreamAdded(uint node_id);
    // org.gnome.Mutter.DisplayConfig::MonitorsChanged - invalidates the cached monitor layout.
    void onMonitorsChanged();

private:
    // Re-reads the monitor layout from DisplayConfig into |monitors_cache_| when it is invalid.
    void refreshMonitors() const;
    // Connector to record: the requested one if still present, otherwise the primary monitor.
    QString connectorToRecord() const;
    void fail();

    // Connection to the session user's bus, authenticated as that user (the host runs as root).
    uid_t session_uid_;
    QString connection_name_;
    QDBusConnection bus_;

    QDBusInterface* screen_cast_ = nullptr;
    QDBusInterface* session_ = nullptr;
    QDBusInterface* remote_desktop_ = nullptr;
    QDBusInterface* remote_desktop_session_ = nullptr;

    QString session_path_;
    QString stream_path_;
    QString remote_desktop_session_path_;
    bool started_ = false;
    Stream stream_;

    // Monitor the client asked to record (empty = use the primary) and the one actually recorded.
    QString requested_connector_;
    QString recorded_connector_;

    // Monitor layout cache, lazily filled and invalidated by the MonitorsChanged signal so the layout
    // is not re-read from D-Bus on every screenCount() call.
    mutable QList<MonitorInfo> monitors_cache_;
    mutable bool monitors_valid_ = false;

    Q_DISABLE_COPY_MOVE(MutterScreenCast)
};

#endif // HOST_LINUX_MUTTER_SCREEN_CAST_H
