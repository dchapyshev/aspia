#
# Aspia Project
# Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_CODEC
    cursor_decoder.cc
    cursor_decoder.h
    cursor_encoder.cc
    cursor_encoder.h
    encoder_bitrate_filter.cc
    encoder_bitrate_filter.h
    pixel_translator.cc
    pixel_translator.h
    running_samples.cc
    running_samples.h
    scale_reducer.cc
    scale_reducer.h
    scoped_vpx_codec.cc
    scoped_vpx_codec.h
    scoped_zstd_stream.cc
    scoped_zstd_stream.h
    video_decoder.cc
    video_decoder.h
    video_decoder_vpx.cc
    video_decoder_vpx.h
    video_decoder_zstd.cc
    video_decoder_zstd.h
    video_encoder.cc
    video_encoder.h
    video_encoder_vpx.cc
    video_encoder_vpx.h
    video_encoder_zstd.cc
    video_encoder_zstd.h
    video_util.cc
    video_util.h
    weighted_samples.cc
    weighted_samples.h)

list(APPEND SOURCE_CODEC_UNIT_TESTS
    running_samples_unittest.cc
    weighted_samples_unittest.cc)

source_group("" FILES ${SOURCE_CODEC} ${SOURCE_CODEC_UNIT_TESTS})

add_library(aspia_codec STATIC ${SOURCE_CODEC})
target_link_libraries(aspia_codec
    aspia_base
    aspia_desktop
    aspia_proto
    ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_codec_tests ${SOURCE_CODEC_UNIT_TESTS})
    target_link_libraries(aspia_codec_tests
        aspia_base
        aspia_codec
        optimized gtest
        optimized gtest_main
        debug gtestd
        debug gtest_maind
        ${WINDOWS_LIBS}
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_codec_tests COMMAND aspia_codec_tests)
endif()
