#
# Aspia Project
# Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

collect_sources(SOURCE_HOST_WIN
    d3d_device.cc
    d3d_device.h
    dxgi_adapter_duplicator.cc
    dxgi_adapter_duplicator.h
    dxgi_context.cc
    dxgi_context.h
    dxgi_duplicator_controller.cc
    dxgi_duplicator_controller.h
    dxgi_frame.cc
    dxgi_frame.h
    dxgi_output_duplicator.cc
    dxgi_output_duplicator.h
    dxgi_texture.cc
    dxgi_texture.h
    dxgi_texture_mapping.cc
    dxgi_texture_mapping.h
    dxgi_texture_staging.cc
    dxgi_texture_staging.h
    screen_capture_utils.cc
    screen_capture_utils.h
    swdevice_defines.h
    touch_injector.cc
    touch_injector.h
    touch_injector_defines.h
    updater_launcher.cc
    updater_launcher.h
    virtual_display.cc
    virtual_display.h)
