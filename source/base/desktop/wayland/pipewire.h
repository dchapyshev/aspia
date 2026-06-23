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

#ifndef BASE_DESKTOP_WAYLAND_PIPEWIRE_H
#define BASE_DESKTOP_WAYLAND_PIPEWIRE_H

#include <QtClassHelperMacros>

#include <pipewire/pipewire.h>

// List of libpipewire functions resolved dynamically. Inline/macro helpers from the PipeWire and
// SPA headers (pod builders, interface calls) are not symbols and are not listed here.
#define ASPIA_PIPEWIRE_SYMBOLS(X) \
    X(pw_init) \
    X(pw_deinit) \
    X(pw_context_new) \
    X(pw_context_destroy) \
    X(pw_context_connect_fd) \
    X(pw_core_disconnect) \
    X(pw_thread_loop_new) \
    X(pw_thread_loop_destroy) \
    X(pw_thread_loop_get_loop) \
    X(pw_thread_loop_start) \
    X(pw_thread_loop_stop) \
    X(pw_thread_loop_lock) \
    X(pw_thread_loop_unlock) \
    X(pw_thread_loop_wait) \
    X(pw_thread_loop_signal) \
    X(pw_properties_new) \
    X(pw_stream_new) \
    X(pw_stream_destroy) \
    X(pw_stream_connect) \
    X(pw_stream_disconnect) \
    X(pw_stream_add_listener) \
    X(pw_stream_dequeue_buffer) \
    X(pw_stream_queue_buffer) \
    X(pw_stream_update_params) \
    X(pw_stream_set_active) \
    X(pw_stream_get_state) \
    X(pw_stream_state_as_string)

// Pointers to the resolved functions. Types are taken from the real PipeWire declarations.
struct PipeWireApi
{
#define ASPIA_PIPEWIRE_DECLARE(sym) decltype(&::sym) sym = nullptr;
    ASPIA_PIPEWIRE_SYMBOLS(ASPIA_PIPEWIRE_DECLARE)
#undef ASPIA_PIPEWIRE_DECLARE
};

// Thin wrapper over libpipewire. The library is loaded dynamically on first use and the resolved
// symbols are cached, so the binary does not link against libpipewire. Call sites use the function
// pointers through api(), e.g. PipeWire::api()->pw_stream_connect(...).
class PipeWire
{
    Q_DISABLE_COPY_MOVE(PipeWire)

public:
    // Loads libpipewire and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    // Resolved function table, or nullptr if the library has not been loaded successfully.
    static const PipeWireApi* api();
};

#endif // BASE_DESKTOP_WAYLAND_PIPEWIRE_H
