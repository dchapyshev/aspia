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

#include "base/desktop/linux/mutter_screen_cast.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusVariant>
#include <QMetaType>

#include "base/logging.h"
#include "base/linux/session_dbus.h"

// org.gnome.Mutter.DisplayConfig.GetCurrentState monitor layout, demarshalled via qdbus_cast (manual
// QDBusArgument navigation does not descend reliably into these nested structures). Only the connector
// of the first monitor is used, but the full structure must be described so the cast consumes it.
struct GnomeMonitorMode
{
    QString id;
    int width = 0;
    int height = 0;
    double refresh_rate = 0.0;
    double preferred_scale = 0.0;
    QList<double> supported_scales;
    QVariantMap properties;
};

struct GnomeMonitor
{
    QString connector;
    QString vendor;
    QString product;
    QString serial;
    QList<GnomeMonitorMode> modes;
    QVariantMap properties;
};

Q_DECLARE_METATYPE(GnomeMonitorMode)
Q_DECLARE_METATYPE(GnomeMonitor)

//--------------------------------------------------------------------------------------------------
QDBusArgument& operator<<(QDBusArgument& argument, const GnomeMonitorMode& mode)
{
    argument.beginStructure();
    argument << mode.id << mode.width << mode.height << mode.refresh_rate << mode.preferred_scale;
    argument.beginArray(QMetaType::fromType<double>());
    for (double scale : mode.supported_scales)
        argument << scale;
    argument.endArray();
    argument << mode.properties;
    argument.endStructure();
    return argument;
}

//--------------------------------------------------------------------------------------------------
const QDBusArgument& operator>>(const QDBusArgument& argument, GnomeMonitorMode& mode)
{
    argument.beginStructure();
    argument >> mode.id >> mode.width >> mode.height >> mode.refresh_rate >> mode.preferred_scale;
    argument.beginArray();
    while (!argument.atEnd())
    {
        double scale = 0.0;
        argument >> scale;
        mode.supported_scales.append(scale);
    }
    argument.endArray();
    argument >> mode.properties;
    argument.endStructure();
    return argument;
}

//--------------------------------------------------------------------------------------------------
QDBusArgument& operator<<(QDBusArgument& argument, const GnomeMonitor& monitor)
{
    argument.beginStructure();
    argument.beginStructure();
    argument << monitor.connector << monitor.vendor << monitor.product << monitor.serial;
    argument.endStructure();
    argument << monitor.modes << monitor.properties;
    argument.endStructure();
    return argument;
}

//--------------------------------------------------------------------------------------------------
const QDBusArgument& operator>>(const QDBusArgument& argument, GnomeMonitor& monitor)
{
    argument.beginStructure();
    argument.beginStructure();
    argument >> monitor.connector >> monitor.vendor >> monitor.product >> monitor.serial;
    argument.endStructure();
    argument >> monitor.modes >> monitor.properties;
    argument.endStructure();
    return argument;
}

namespace {

const char kService[] = "org.gnome.Mutter.ScreenCast";
const char kScreenCastPath[] = "/org/gnome/Mutter/ScreenCast";
const char kScreenCastIface[] = "org.gnome.Mutter.ScreenCast";
const char kSessionIface[] = "org.gnome.Mutter.ScreenCast.Session";
const char kStreamIface[] = "org.gnome.Mutter.ScreenCast.Stream";

const char kDisplayService[] = "org.gnome.Mutter.DisplayConfig";
const char kDisplayPath[] = "/org/gnome/Mutter/DisplayConfig";
const char kDisplayIface[] = "org.gnome.Mutter.DisplayConfig";

const char kRemoteDesktopService[] = "org.gnome.Mutter.RemoteDesktop";
const char kRemoteDesktopPath[] = "/org/gnome/Mutter/RemoteDesktop";
const char kRemoteDesktopIface[] = "org.gnome.Mutter.RemoteDesktop";
const char kRemoteDesktopSessionIface[] = "org.gnome.Mutter.RemoteDesktop.Session";
const char kPropertiesIface[] = "org.freedesktop.DBus.Properties";

// org.gnome.Mutter.ScreenCast cursor mode: deliver the cursor as separate metadata (not baked into
// the frames) so the client draws a single cursor.
const uint kCursorModeMetadata = 2;

} // namespace

//--------------------------------------------------------------------------------------------------
MutterScreenCast::MutterScreenCast(uid_t session_uid, QObject* parent)
    : WaylandCompositorSource(parent),
      session_uid_(session_uid),
      connection_name_(QString("aspia-mutter-%1").arg(session_uid)),
      bus_(SessionDBus::connectAsUser(session_uid, connection_name_))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
MutterScreenCast::~MutterScreenCast()
{
    LOG(INFO) << "Dtor";

    if (remote_desktop_session_)
        remote_desktop_session_->call("Stop");
    if (session_)
        session_->call("Stop");

    if (bus_.isConnected())
        QDBusConnection::disconnectFromBus(connection_name_);
}

//--------------------------------------------------------------------------------------------------
bool MutterScreenCast::isAvailable() const
{
    if (!bus_.isConnected() || !bus_.interface())
        return false;

    return bus_.interface()->isServiceRegistered(kService).value();
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::start()
{
    // Tear down any previous session so start() can be called again to renegotiate (e.g. after a
    // monitor reconfiguration created a new node).
    if (remote_desktop_session_)
        remote_desktop_session_->call("Stop");
    if (session_)
        session_->call("Stop");
    delete remote_desktop_session_;
    delete remote_desktop_;
    delete session_;
    delete screen_cast_;
    remote_desktop_session_ = nullptr;
    remote_desktop_ = nullptr;
    session_ = nullptr;
    screen_cast_ = nullptr;
    started_ = false;
    stream_ = Stream();

    const QString connector = primaryConnector();
    if (connector.isEmpty())
    {
        LOG(ERROR) << "No monitor connector available";
        fail();
        return;
    }

    QDBusConnection bus = bus_;

    // RemoteDesktop session carries input. Create it first so the ScreenCast session can be linked to
    // it (input is delivered against the stream of the linked screen-cast session).
    remote_desktop_ =
        new QDBusInterface(kRemoteDesktopService, kRemoteDesktopPath, kRemoteDesktopIface, bus, this);
    QDBusReply<QDBusObjectPath> remote_desktop_reply = remote_desktop_->call("CreateSession");
    if (!remote_desktop_reply.isValid())
    {
        LOG(ERROR) << "RemoteDesktop.CreateSession failed:" << remote_desktop_reply.error().message();
        fail();
        return;
    }
    remote_desktop_session_path_ = remote_desktop_reply.value().path();
    remote_desktop_session_ = new QDBusInterface(
        kRemoteDesktopService, remote_desktop_session_path_, kRemoteDesktopSessionIface, bus, this);

    QString remote_desktop_session_id;
    {
        QDBusInterface properties(
            kRemoteDesktopService, remote_desktop_session_path_, kPropertiesIface, bus);
        QDBusReply<QDBusVariant> id_reply =
            properties.call("Get", kRemoteDesktopSessionIface, "SessionId");
        if (id_reply.isValid())
            remote_desktop_session_id = id_reply.value().variant().toString();
    }

    // ScreenCast session, linked to the RemoteDesktop session so input targets its stream.
    screen_cast_ = new QDBusInterface(kService, kScreenCastPath, kScreenCastIface, bus, this);

    QVariantMap session_properties;
    if (!remote_desktop_session_id.isEmpty())
        session_properties.insert("remote-desktop-session-id", remote_desktop_session_id);

    QDBusReply<QDBusObjectPath> session_reply =
        screen_cast_->call("CreateSession", session_properties);
    if (!session_reply.isValid())
    {
        LOG(ERROR) << "ScreenCast.CreateSession failed:" << session_reply.error().message();
        fail();
        return;
    }
    session_path_ = session_reply.value().path();

    session_ = new QDBusInterface(kService, session_path_, kSessionIface, bus, this);

    QVariantMap properties;
    properties.insert("cursor-mode", QVariant::fromValue(kCursorModeMetadata));

    QDBusReply<QDBusObjectPath> stream_reply =
        session_->call("RecordMonitor", connector, properties);
    if (!stream_reply.isValid())
    {
        LOG(ERROR) << "RecordMonitor failed:" << stream_reply.error().message();
        fail();
        return;
    }
    stream_path_ = stream_reply.value().path();

    bus.connect(kService, stream_path_, kStreamIface, "PipeWireStreamAdded",
                this, SLOT(onPipeWireStreamAdded(uint)));

    // Starting the RemoteDesktop session also starts the linked ScreenCast stream.
    QDBusReply<void> start_reply = remote_desktop_session_->call("Start");
    if (!start_reply.isValid())
    {
        LOG(ERROR) << "RemoteDesktop.Session.Start failed:" << start_reply.error().message();
        fail();
        return;
    }

    LOG(INFO) << "Mutter screen cast started, waiting for node (connector:" << connector << ")";
}

//--------------------------------------------------------------------------------------------------
bool MutterScreenCast::isStarted() const
{
    return started_;
}

//--------------------------------------------------------------------------------------------------
const WaylandCaptureSource::Stream& MutterScreenCast::stream() const
{
    return stream_;
}

//--------------------------------------------------------------------------------------------------
int MutterScreenCast::pipeWireFd() const
{
    // The node lives on the session's own PipeWire daemon; the consumer connects to the default socket.
    return -1;
}

//--------------------------------------------------------------------------------------------------
uid_t MutterScreenCast::pipeWireUid() const
{
    // The node is on this user's PipeWire daemon; the consumer connects to its socket explicitly.
    return session_uid_;
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::notifyPointerMotionAbsolute(double x, double y)
{
    if (!started_ || !remote_desktop_session_)
        return;
    remote_desktop_session_->asyncCall("NotifyPointerMotionAbsolute", stream_path_, x, y);
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::notifyPointerButton(qint32 button, bool pressed)
{
    if (!started_ || !remote_desktop_session_)
        return;
    remote_desktop_session_->asyncCall("NotifyPointerButton", button, pressed);
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::notifyPointerAxis(double dx, double dy)
{
    if (!started_ || !remote_desktop_session_)
        return;
    remote_desktop_session_->asyncCall("NotifyPointerAxis", dx, dy, quint32(0));
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::notifyPointerAxisDiscrete(quint32 axis, qint32 steps)
{
    if (!started_ || !remote_desktop_session_)
        return;
    remote_desktop_session_->asyncCall("NotifyPointerAxisDiscrete", axis, steps);
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::notifyKeyboardKeycode(qint32 keycode, bool pressed)
{
    if (!started_ || !remote_desktop_session_)
        return;
    remote_desktop_session_->asyncCall("NotifyKeyboardKeycode", quint32(keycode), pressed);
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::notifyKeyboardKeysym(qint32 keysym, bool pressed)
{
    if (!started_ || !remote_desktop_session_)
        return;
    remote_desktop_session_->asyncCall("NotifyKeyboardKeysym", quint32(keysym), pressed);
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::onPipeWireStreamAdded(uint node_id)
{
    LOG(INFO) << "PipeWire stream added, node:" << node_id;
    stream_.node_id = node_id;

    // The stream size is filled in from the first PipeWire frame by the capturer; no need to read it
    // from the Parameters property here.
    started_ = true;
    LOG(INFO) << "Mutter screen cast ready (node:" << stream_.node_id << ")";
    emit sig_started(true);
}

//--------------------------------------------------------------------------------------------------
QString MutterScreenCast::primaryConnector() const
{
    qDBusRegisterMetaType<GnomeMonitorMode>();
    qDBusRegisterMetaType<QList<GnomeMonitorMode>>();
    qDBusRegisterMetaType<GnomeMonitor>();
    qDBusRegisterMetaType<QList<GnomeMonitor>>();

    QDBusInterface display(kDisplayService, kDisplayPath, kDisplayIface, bus_);
    QDBusMessage reply = display.call("GetCurrentState");
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 2)
    {
        LOG(ERROR) << "GetCurrentState failed:" << reply.errorMessage();
        return QString();
    }

    // Reply layout: (u serial, a((ssss)a(siiddada{sv})a{sv}) monitors, ...). Return the connector of
    // the first monitor.
    const QList<GnomeMonitor> monitors = qdbus_cast<QList<GnomeMonitor>>(reply.arguments().at(1));
    if (monitors.isEmpty())
    {
        LOG(ERROR) << "No monitors in GetCurrentState reply";
        return QString();
    }

    const QString connector = monitors.first().connector;
    LOG(INFO) << "Primary monitor connector:" << connector << "(monitors:" << monitors.size() << ")";
    return connector;
}

//--------------------------------------------------------------------------------------------------
void MutterScreenCast::fail()
{
    started_ = false;
    emit sig_started(false);
}
