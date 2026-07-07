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

#include "host/linux/wayland_output_layout.h"

#include <wayland-client.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <vector>

#include "base/logging.h"

#include "xdg-output-client-protocol.h"

namespace {

//--------------------------------------------------------------------------------------------------
struct OutputData
{
    wl_output* output = nullptr;
    zxdg_output_v1* xdg = nullptr;
    QString name;
    QSize physical;
    QPoint logical_pos;
    QSize logical_size;
};

//--------------------------------------------------------------------------------------------------
struct State
{
    zxdg_output_manager_v1* xdg_manager = nullptr;
    std::vector<OutputData*> outputs;
};

//--------------------------------------------------------------------------------------------------
void outputGeometry(void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t,
                    const char*, const char*, int32_t)
{
    // Not used.
}

//--------------------------------------------------------------------------------------------------
void outputMode(void* data, wl_output*, uint32_t flags, int32_t width, int32_t height, int32_t)
{
    if (flags & WL_OUTPUT_MODE_CURRENT)
        static_cast<OutputData*>(data)->physical = QSize(width, height);
}

//--------------------------------------------------------------------------------------------------
void outputDone(void*, wl_output*)
{
    // Not used.
}

//--------------------------------------------------------------------------------------------------
void outputScale(void*, wl_output*, int32_t)
{
    // Integer scale is insufficient for fractional scaling; the logical size comes from xdg-output.
}

//--------------------------------------------------------------------------------------------------
void outputName(void* data, wl_output*, const char* name)
{
    static_cast<OutputData*>(data)->name = QString::fromUtf8(name);
}

//--------------------------------------------------------------------------------------------------
void outputDescription(void*, wl_output*, const char*)
{
    // Not used.
}

const wl_output_listener kOutputListener =
{
    outputGeometry, outputMode, outputDone, outputScale, outputName, outputDescription
};

//--------------------------------------------------------------------------------------------------
void xdgLogicalPosition(void* data, zxdg_output_v1*, int32_t x, int32_t y)
{
    static_cast<OutputData*>(data)->logical_pos = QPoint(x, y);
}

//--------------------------------------------------------------------------------------------------
void xdgLogicalSize(void* data, zxdg_output_v1*, int32_t width, int32_t height)
{
    static_cast<OutputData*>(data)->logical_size = QSize(width, height);
}

//--------------------------------------------------------------------------------------------------
void xdgDone(void*, zxdg_output_v1*)
{
    // Not used.
}

//--------------------------------------------------------------------------------------------------
void xdgName(void* data, zxdg_output_v1*, const char* name)
{
    OutputData* output_data = static_cast<OutputData*>(data);
    if (output_data->name.isEmpty())
        output_data->name = QString::fromUtf8(name);
}

//--------------------------------------------------------------------------------------------------
void xdgDescription(void*, zxdg_output_v1*, const char*)
{
    // Not used.
}

const zxdg_output_v1_listener kXdgOutputListener =
{
    xdgLogicalPosition, xdgLogicalSize, xdgDone, xdgName, xdgDescription
};

//--------------------------------------------------------------------------------------------------
void registryGlobal(void* data, wl_registry* registry, uint32_t id, const char* interface,
                    uint32_t version)
{
    State* state = static_cast<State*>(data);

    if (strcmp(interface, wl_output_interface.name) == 0)
    {
        // Version 4 is required for the output name event used to match against DRM connectors.
        const uint32_t bind_version = version < 4 ? version : 4;

        OutputData* output_data = new OutputData();
        output_data->output = static_cast<wl_output*>(
            wl_registry_bind(registry, id, &wl_output_interface, bind_version));
        wl_output_add_listener(output_data->output, &kOutputListener, output_data);
        state->outputs.push_back(output_data);
    }
    else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0)
    {
        const uint32_t bind_version = version < 3 ? version : 3;
        state->xdg_manager = static_cast<zxdg_output_manager_v1*>(
            wl_registry_bind(registry, id, &zxdg_output_manager_v1_interface, bind_version));
    }
}

//--------------------------------------------------------------------------------------------------
void registryGlobalRemove(void*, wl_registry*, uint32_t)
{
    // Not used.
}

const wl_registry_listener kRegistryListener = { registryGlobal, registryGlobalRemove };

//--------------------------------------------------------------------------------------------------
int connectSocket(const QString& path)
{
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    const QByteArray local_path = path.toLocal8Bit();
    if (local_path.size() >= static_cast<int>(sizeof(addr.sun_path)))
    {
        ::close(fd);
        return -1;
    }
    strcpy(addr.sun_path, local_path.constData());

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        ::close(fd);
        return -1;
    }

    return fd;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
QList<WaylandOutputLayout::Output> WaylandOutputLayout::query(const QString& socket_path)
{
    QList<Output> result;

    int fd = connectSocket(socket_path);
    if (fd < 0)
    {
        LOG(INFO) << "Wayland: unable to connect to socket" << socket_path;
        return result;
    }

    // wl_display_connect_to_fd() takes ownership of the descriptor.
    wl_display* display = wl_display_connect_to_fd(fd);
    if (!display)
    {
        ::close(fd);
        return result;
    }

    State state;
    wl_registry* registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &kRegistryListener, &state);
    wl_display_roundtrip(display);

    if (state.xdg_manager)
    {
        for (OutputData* output_data : state.outputs)
        {
            output_data->xdg =
                zxdg_output_manager_v1_get_xdg_output(state.xdg_manager, output_data->output);
            zxdg_output_v1_add_listener(output_data->xdg, &kXdgOutputListener, output_data);
        }
        wl_display_roundtrip(display);
    }

    for (OutputData* output_data : state.outputs)
    {
        if (output_data->logical_size.isValid())
        {
            Output output;
            output.name = output_data->name;
            output.physical = output_data->physical;
            output.logical = QRect(output_data->logical_pos, output_data->logical_size);
            result.append(output);
        }

        if (output_data->xdg)
            zxdg_output_v1_destroy(output_data->xdg);
        if (output_data->output)
            wl_output_destroy(output_data->output);
        delete output_data;
    }

    if (state.xdg_manager)
        zxdg_output_manager_v1_destroy(state.xdg_manager);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);

    return result;
}
