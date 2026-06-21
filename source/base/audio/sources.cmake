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

collect_sources(SOURCE_BASE_AUDIO
    audio_capturer.cc
    audio_capturer.h
    audio_capturer_wrapper.cc
    audio_capturer_wrapper.h
    audio_output.cc
    audio_output.h
    audio_player.cc
    audio_player.h
    audio_silence_detector.cc
    audio_silence_detector.h
    audio_volume_filter.cc
    audio_volume_filter.h)

if (WIN32)
    collect_sources(SOURCE_BASE_AUDIO
        audio_capturer_win.cc
        audio_capturer_win.h
        audio_output_win.cc
        audio_output_win.h
        audio_volume_filter_win.cc
        audio_volume_filter_win.h)
endif()

if (LINUX)
    collect_sources(SOURCE_BASE_AUDIO
        audio_capturer_linux.cc
        audio_capturer_linux.h
        audio_output_pulse.cc
        audio_output_pulse.h)
endif()

if (APPLE)
    collect_sources(SOURCE_BASE_AUDIO
        audio_output_mac.cc
        audio_output_mac.h)
endif()

if (ANDROID)
    collect_sources(SOURCE_BASE_AUDIO
        audio_output_android.cc
        audio_output_android.h)
endif()
