#
# Aspia Project
# Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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
    compressor.h
    compressor_zstd.cc
    compressor_zstd.h
    cursor_decoder.cc
    cursor_decoder.h
    cursor_encoder.cc
    cursor_encoder.h
    decompressor.h
    decompressor_zstd.cc
    decompressor_zstd.h
    pixel_translator.cc
    pixel_translator.h
    scale_reducer.cc
    scale_reducer.h
    scoped_vpx_codec.cc
    scoped_vpx_codec.h
    scoped_zstd_dict.cc
    scoped_zstd_dict.h
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
    video_util.h)

list(APPEND SOURCE_CODEC_UNIT_TESTS
    compressor_zstd_unittest.cc)

source_group("" FILES ${SOURCE_CODEC})

add_library(aspia_codec STATIC ${SOURCE_CODEC})
target_link_libraries(aspia_codec
    aspia_base
    aspia_protocol
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
        ${THIRD_PARTY_LIBS})

    # Get the list of files containing messages.
    file(GLOB_RECURSE CODEC_TEST_DATA test_data/*.*)

    foreach(TEST_DATA_FILE ${CODEC_TEST_DATA})
        get_filename_component(TEST_DATA_FILE_NAME ${TEST_DATA_FILE} NAME)
        add_custom_command(TARGET aspia_codec_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${TEST_DATA_FILE}
            ${CMAKE_BINARY_DIR}/$<CONFIG>/test_data/${TEST_DATA_FILE_NAME})
    endforeach()

    add_test(NAME aspia_codec_tests COMMAND aspia_codec_tests)
endif()

