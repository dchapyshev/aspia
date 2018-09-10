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
    ${PROJECT_SOURCE_DIR}/codec/compressor.h
    ${PROJECT_SOURCE_DIR}/codec/compressor_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/compressor_zlib.h
    ${PROJECT_SOURCE_DIR}/codec/cursor_decoder.cc
    ${PROJECT_SOURCE_DIR}/codec/cursor_decoder.h
    ${PROJECT_SOURCE_DIR}/codec/cursor_encoder.cc
    ${PROJECT_SOURCE_DIR}/codec/cursor_encoder.h
    ${PROJECT_SOURCE_DIR}/codec/decompressor.h
    ${PROJECT_SOURCE_DIR}/codec/decompressor_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/decompressor_zlib.h
    ${PROJECT_SOURCE_DIR}/codec/pixel_translator.cc
    ${PROJECT_SOURCE_DIR}/codec/pixel_translator.h
    ${PROJECT_SOURCE_DIR}/codec/scale_reducer.cc
    ${PROJECT_SOURCE_DIR}/codec/scale_reducer.h
    ${PROJECT_SOURCE_DIR}/codec/scoped_vpx_codec.cc
    ${PROJECT_SOURCE_DIR}/codec/scoped_vpx_codec.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_vpx.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_vpx.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_zlib.h
    ${PROJECT_SOURCE_DIR}/codec/video_encoder.cc
    ${PROJECT_SOURCE_DIR}/codec/video_encoder.h
    ${PROJECT_SOURCE_DIR}/codec/video_encoder_vpx.cc
    ${PROJECT_SOURCE_DIR}/codec/video_encoder_vpx.h
    ${PROJECT_SOURCE_DIR}/codec/video_encoder_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/video_encoder_zlib.h
    ${PROJECT_SOURCE_DIR}/codec/video_util.cc
    ${PROJECT_SOURCE_DIR}/codec/video_util.h)

source_group("" FILES ${SOURCE_CODEC})

add_library(aspia_codec STATIC ${SOURCE_CODEC})
target_link_libraries(aspia_codec
    aspia_base
    aspia_protocol
    ${THIRD_PARTY_LIBS})

