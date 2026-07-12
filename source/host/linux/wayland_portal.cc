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

#include "host/linux/wayland_portal.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDBusVariant>

#include <unistd.h>

#include "base/logging.h"
#include "base/linux/session_dbus.h"

namespace {

const char kPortalService[] = "org.freedesktop.portal.Desktop";
const char kPortalObject[] = "/org/freedesktop/portal/desktop";
const char kScreenCastIface[] = "org.freedesktop.portal.ScreenCast";
const char kRemoteDesktopIface[] = "org.freedesktop.portal.RemoteDesktop";
const char kRequestIface[] = "org.freedesktop.portal.Request";
const char kSessionIface[] = "org.freedesktop.portal.Session";

// RemoteDesktop device types.
const quint32 kDeviceKeyboard = 1;
const quint32 kDevicePointer = 2;

// ScreenCast source types.
const quint32 kSourceMonitor = 1;

// ScreenCast cursor modes.
const quint32 kCursorHidden = 1;
const quint32 kCursorEmbedded = 2;
const quint32 kCursorMetadata = 4;

// Persist the grant until the user revokes it.
const quint32 kPersistModePersistent = 2;

// RemoteDesktop owns the combined session; persistence (persist_mode/restore_token) is available
// from RemoteDesktop version 2.
const quint32 kRemoteDesktopPersistVersion = 2;

//--------------------------------------------------------------------------------------------------
QPoint pointFromVariant(const QVariant& value)
{
    const QDBusArgument arg = value.value<QDBusArgument>();
    int x = 0;
    int y = 0;
    arg.beginStructure();
    arg >> x >> y;
    arg.endStructure();
    return QPoint(x, y);
}

//--------------------------------------------------------------------------------------------------
QSize sizeFromVariant(const QVariant& value)
{
    const QDBusArgument arg = value.value<QDBusArgument>();
    int w = 0;
    int h = 0;
    arg.beginStructure();
    arg >> w >> h;
    arg.endStructure();
    return QSize(w, h);
}

} // namespace

//--------------------------------------------------------------------------------------------------
WaylandPortal::WaylandPortal(uid_t session_uid, QObject* parent)
    : WaylandCompositorSource(parent),
      connection_name_(QString("aspia-portal-%1").arg(session_uid)),
      bus_(SessionDBus::connectAsUser(session_uid, connection_name_))
{
    screen_cast_ = new QDBusInterface(
        kPortalService, kPortalObject, kScreenCastIface, bus_, this);
    remote_desktop_ = new QDBusInterface(
        kPortalService, kPortalObject, kRemoteDesktopIface, bus_, this);

    // The request/session handle paths embed the unique connection name with the leading ':'
    // removed and dots replaced by underscores.
    sender_name_ = bus_.baseService();
    if (sender_name_.startsWith(':'))
        sender_name_ = sender_name_.mid(1);
    sender_name_.replace('.', '_');
}

//--------------------------------------------------------------------------------------------------
WaylandPortal::~WaylandPortal()
{
    closeSession();

    if (bus_.isConnected())
        QDBusConnection::disconnectFromBus(connection_name_);
}

//--------------------------------------------------------------------------------------------------
// static
bool WaylandPortal::isScreenCastAvailable(uid_t session_uid)
{
    const QString name = QString("aspia-portal-probe-%1").arg(session_uid);
    QDBusConnection bus = SessionDBus::connectAsUser(session_uid, name);

    // The ScreenCast portal is backed (a working backend like xdg-desktop-portal-wlr) only when it
    // advertises at least one source type; the gtk-only setup leaves the property unreadable.
    bool available = false;
    if (bus.isConnected())
    {
        // Use a raw message, not QDBusInterface: its constructor does a blocking introspection with the
        // default timeout, and this probe activates xdg-desktop-portal.service - on an absent or broken
        // backend that would block setupLinuxCapture for ~50 s. The portal is a last resort, so cap the
        // call with a short timeout and fail fast.
        QDBusMessage message = QDBusMessage::createMethodCall(
            kPortalService, kPortalObject, "org.freedesktop.DBus.Properties", "Get");
        message << QString(kScreenCastIface) << QString("AvailableSourceTypes");

        const QDBusMessage reply = bus.call(message, QDBus::Block, 3000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
            available = reply.arguments().at(0).value<QDBusVariant>().variant().toUInt() != 0;
    }

    if (bus.isConnected())
        QDBusConnection::disconnectFromBus(name);
    return available;
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::start()
{
    closeSession();
    queryCapabilities();
    createSession();
}

//--------------------------------------------------------------------------------------------------
bool WaylandPortal::isStarted() const
{
    return step_ == Step::STARTED;
}

//--------------------------------------------------------------------------------------------------
int WaylandPortal::pipeWireFd() const
{
    return pipewire_fd_;
}

//--------------------------------------------------------------------------------------------------
const WaylandPortal::Stream& WaylandPortal::stream() const
{
    return stream_;
}

//--------------------------------------------------------------------------------------------------
QString WaylandPortal::restoreToken() const
{
    return restore_token_;
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::notifyPointerMotionAbsolute(double x, double y)
{
    if (step_ != Step::STARTED)
        return;

    remote_desktop_->asyncCallWithArgumentList("NotifyPointerMotionAbsolute",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), QVariantMap(),
          stream_.node_id, x, y });
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::notifyPointerButton(qint32 button, bool pressed)
{
    if (step_ != Step::STARTED)
        return;

    remote_desktop_->asyncCallWithArgumentList("NotifyPointerButton",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), QVariantMap(),
          button, static_cast<uint>(pressed ? 1 : 0) });
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::notifyPointerAxis(double dx, double dy)
{
    if (step_ != Step::STARTED)
        return;

    remote_desktop_->asyncCallWithArgumentList("NotifyPointerAxis",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), QVariantMap(), dx, dy });
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::notifyPointerAxisDiscrete(quint32 axis, qint32 steps)
{
    if (step_ != Step::STARTED)
        return;

    remote_desktop_->asyncCallWithArgumentList("NotifyPointerAxisDiscrete",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), QVariantMap(), axis, steps });
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::notifyKeyboardKeycode(qint32 keycode, bool pressed)
{
    if (step_ != Step::STARTED)
        return;

    remote_desktop_->asyncCallWithArgumentList("NotifyKeyboardKeycode",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), QVariantMap(),
          keycode, static_cast<uint>(pressed ? 1 : 0) });
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::notifyKeyboardKeysym(qint32 keysym, bool pressed)
{
    if (step_ != Step::STARTED)
        return;

    remote_desktop_->asyncCallWithArgumentList("NotifyKeyboardKeysym",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), QVariantMap(),
          keysym, static_cast<uint>(pressed ? 1 : 0) });
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::onResponse(uint response, const QVariantMap& results)
{
    LOG(INFO) << "Portal Response received (step:" << static_cast<int>(step_)
              << "response:" << response << ")";

    bus_.disconnect(
        kPortalService, pending_request_path_, kRequestIface, "Response",
        this, SLOT(onResponse(uint, QVariantMap)));

    if (response != 0)
    {
        // 1 = cancelled by the user, 2 = ended by another reason.
        LOG(ERROR) << "Portal request rejected (step:" << static_cast<int>(step_)
                   << "response:" << response << ")";
        fail();
        return;
    }

    switch (step_)
    {
        case Step::CREATE_SESSION:
            session_handle_ = results.value("session_handle").toString();
            if (session_handle_.isEmpty())
            {
                LOG(ERROR) << "Portal returned an empty session handle";
                fail();
                return;
            }
            // Watch for the session being closed by the compositor or the user.
            bus_.connect(
                kPortalService, session_handle_, kSessionIface, "Closed",
                this, SLOT(onSessionClosed(QVariantMap)));
            selectDevices();
            break;

        case Step::SELECT_DEVICES:
            selectSources();
            break;

        case Step::SELECT_SOURCES:
            startSession();
            break;

        case Step::START:
        {
            restore_token_ = results.value("restore_token").toString();

            const QDBusArgument streams = results.value("streams").value<QDBusArgument>();
            streams.beginArray();
            if (streams.atEnd())
            {
                LOG(ERROR) << "Portal returned no streams";
                streams.endArray();
                fail();
                return;
            }

            streams.beginStructure();
            uint node_id = 0;
            QVariantMap properties;
            streams >> node_id >> properties;
            streams.endStructure();
            streams.endArray();

            stream_.node_id = node_id;
            if (properties.contains("size"))
                stream_.size = sizeFromVariant(properties.value("size"));
            if (properties.contains("position"))
                stream_.position = pointFromVariant(properties.value("position"));

            openPipeWireRemote();
        }
        break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::onSessionClosed(const QVariantMap& /* details */)
{
    LOG(INFO) << "Wayland portal session closed";

    bus_.disconnect(
        kPortalService, session_handle_, kSessionIface, "Closed",
        this, SLOT(onSessionClosed(QVariantMap)));

    // The session is gone; the handle must not be closed again from the destructor.
    session_handle_.clear();
    step_ = Step::FAILED;

    emit sig_closed();
}

//--------------------------------------------------------------------------------------------------
QString WaylandPortal::newToken(const QString& prefix)
{
    return prefix + QString::number(++token_counter_);
}

//--------------------------------------------------------------------------------------------------
QString WaylandPortal::prepareRequest(const QString& token_prefix)
{
    const QString token = newToken(token_prefix);
    pending_request_path_ = QString("/org/freedesktop/portal/desktop/request/%1/%2")
        .arg(sender_name_, token);

    bus_.connect(
        kPortalService, pending_request_path_, kRequestIface, "Response",
        this, SLOT(onResponse(uint, QVariantMap)));

    return token;
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::issueRequest(QDBusInterface* interface, const QString& method,
                                 const QVariantList& arguments, const char* description)
{
    QDBusPendingCall call = interface->asyncCallWithArgumentList(method, arguments);
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(call, this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, description](
        QDBusPendingCallWatcher* w)
    {
        w->deleteLater();

        // The real result is delivered to onResponse via the request's Response signal; here we only
        // detect a failure of the call itself.
        const QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError())
        {
            LOG(ERROR) << description << "failed:" << reply.error().message();
            fail();
        }
    });
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::queryCapabilities()
{
    screen_cast_version_ = portalProperty(kScreenCastIface, "version");
    available_source_types_ = portalProperty(kScreenCastIface, "AvailableSourceTypes");
    available_cursor_modes_ = portalProperty(kScreenCastIface, "AvailableCursorModes");

    remote_desktop_version_ = portalProperty(kRemoteDesktopIface, "version");
    available_device_types_ = portalProperty(kRemoteDesktopIface, "AvailableDeviceTypes");

    LOG(INFO) << "Portal capabilities (ScreenCast v" << screen_cast_version_ << ", RemoteDesktop v"
              << remote_desktop_version_ << ", cursor modes:" << available_cursor_modes_
              << ", source types:" << available_source_types_
              << ", device types:" << available_device_types_ << ")";
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::createSession()
{
    step_ = Step::CREATE_SESSION;

    QVariantMap options;
    options["handle_token"] = prepareRequest("aspia_req");
    options["session_handle_token"] = newToken("aspia_ses");

    issueRequest(remote_desktop_, "CreateSession", { options }, "CreateSession");
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::selectDevices()
{
    step_ = Step::SELECT_DEVICES;

    quint32 device_types = kDeviceKeyboard | kDevicePointer;
    if (available_device_types_)
        device_types &= available_device_types_;

    QVariantMap options;
    options["handle_token"] = prepareRequest("aspia_req");
    options["types"] = device_types;

    // Persistence is only available from RemoteDesktop version 2.
    if (remote_desktop_version_ >= kRemoteDesktopPersistVersion)
    {
        options["persist_mode"] = kPersistModePersistent;
        if (!restore_token_in_.isEmpty())
            options["restore_token"] = restore_token_in_;
    }

    issueRequest(remote_desktop_, "SelectDevices",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), options }, "SelectDevices");
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::selectSources()
{
    step_ = Step::SELECT_SOURCES;

    // Prefer separate cursor metadata, fall back to an embedded cursor, then to a hidden one.
    quint32 cursor_mode = kCursorHidden;
    if (available_cursor_modes_ & kCursorMetadata)
        cursor_mode = kCursorMetadata;
    else if (available_cursor_modes_ & kCursorEmbedded)
        cursor_mode = kCursorEmbedded;

    quint32 source_types = kSourceMonitor;
    if (available_source_types_)
        source_types &= available_source_types_;

    QVariantMap options;
    options["handle_token"] = prepareRequest("aspia_req");
    options["types"] = source_types;
    options["multiple"] = false;
    options["cursor_mode"] = cursor_mode;

    // Persistence of the combined session is governed by RemoteDesktop (it owns the session), so it
    // is requested in SelectDevices rather than here, to avoid restoring screen capture without the
    // matching input grant.

    issueRequest(screen_cast_, "SelectSources",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), options }, "SelectSources");
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::startSession()
{
    step_ = Step::START;

    QVariantMap options;
    options["handle_token"] = prepareRequest("aspia_req");

    issueRequest(remote_desktop_, "Start",
        { QVariant::fromValue(QDBusObjectPath(session_handle_)), QString(), options }, "Start");
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::openPipeWireRemote()
{
    QDBusReply<QDBusUnixFileDescriptor> reply = screen_cast_->call(
        "OpenPipeWireRemote", QVariant::fromValue(QDBusObjectPath(session_handle_)), QVariantMap());
    if (!reply.isValid())
    {
        LOG(ERROR) << "OpenPipeWireRemote failed:" << reply.error().message();
        fail();
        return;
    }

    pipewire_fd_ = ::dup(reply.value().fileDescriptor());
    if (pipewire_fd_ < 0)
    {
        LOG(ERROR) << "Unable to duplicate the PipeWire file descriptor";
        fail();
        return;
    }

    step_ = Step::STARTED;
    LOG(INFO) << "Wayland portal session started (node:" << stream_.node_id
              << "size:" << stream_.size << ")";
    emit sig_started(true);
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::fail()
{
    step_ = Step::FAILED;
    emit sig_started(false);
}

//--------------------------------------------------------------------------------------------------
void WaylandPortal::closeSession()
{
    if (pipewire_fd_ >= 0)
    {
        ::close(pipewire_fd_);
        pipewire_fd_ = -1;
    }

    // If a negotiation was interrupted mid-flight, its Response subscription is still live (a
    // repeated disconnect after a delivered response is a harmless no-op).
    if (!pending_request_path_.isEmpty())
    {
        bus_.disconnect(
            kPortalService, pending_request_path_, kRequestIface, "Response",
            this, SLOT(onResponse(uint, QVariantMap)));
        pending_request_path_.clear();
    }

    if (!session_handle_.isEmpty())
    {
        bus_.disconnect(
            kPortalService, session_handle_, kSessionIface, "Closed",
            this, SLOT(onSessionClosed(QVariantMap)));

        QDBusInterface session(kPortalService, session_handle_, kSessionIface, bus_);
        session.call("Close");
        session_handle_.clear();
    }

    stream_ = Stream();
    step_ = Step::IDLE;
}

//--------------------------------------------------------------------------------------------------
quint32 WaylandPortal::portalProperty(const QString& interface, const QString& property) const
{
    QDBusInterface properties(
        kPortalService, kPortalObject, "org.freedesktop.DBus.Properties",
        bus_);

    QDBusReply<QDBusVariant> reply = properties.call("Get", interface, property);
    if (!reply.isValid())
        return 0;

    return reply.value().variant().toUInt();
}
