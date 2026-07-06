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

collect_sources(SOURCE_BASE_CODEC
    audio_bus.cc
    audio_bus.h
    audio_decoder.cc
    audio_decoder.h
    audio_encoder.cc
    audio_encoder.h
    audio_sample_types.h
    cursor_decoder.cc
    cursor_decoder.h
    cursor_encoder.cc
    cursor_encoder.h
    multi_channel_resampler.cc
    multi_channel_resampler.h
    scale_reducer.cc
    scale_reducer.h
    scoped_vpx_codec.cc
    scoped_vpx_codec.h
    scoped_zstd_stream.cc
    scoped_zstd_stream.h
    sinc_resampler.cc
    sinc_resampler.h
    vector_math.cc
    vector_math.h
    video_decoder.cc
    video_decoder.h
    video_decoder_h264_sw.cc
    video_decoder_h264_sw.h
    video_decoder_vpx.cc
    video_decoder_vpx.h
    video_encoder.cc
    video_encoder.h
    video_encoder_vpx.cc
    video_encoder_vpx.h
    webm_file_muxer.cc
    webm_file_muxer.h
    webm_file_writer.cc
    webm_file_writer.h
    webm_video_encoder.cc
    webm_video_encoder.h
    yuv_converter.cc
    yuv_converter.h
    zstd_compress.cc
    zstd_compress.h
    zstd_stream_compressor.cc
    zstd_stream_compressor.h
    zstd_stream_decompressor.cc
    zstd_stream_decompressor.h)

if (WIN32)
    collect_sources(SOURCE_BASE_CODEC
        d3d11_video_context.cc
        d3d11_video_context.h
        mf_runtime.cc
        mf_runtime.h
        video_decoder_h264_mf.cc
        video_decoder_h264_mf.h
        video_encoder_h264_mf.cc
        video_encoder_h264_mf.h)
endif()

if (APPLE)
    collect_sources(SOURCE_BASE_CODEC
        video_decoder_h264_vt.cc
        video_decoder_h264_vt.h
        video_encoder_h264_vt.cc
        video_encoder_h264_vt.h)
endif()

if (ANDROID)
    collect_sources(SOURCE_BASE_CODEC
        video_decoder_h264_mc.cc
        video_decoder_h264_mc.h
        video_encoder_h264_mc.cc
        video_encoder_h264_mc.h)
endif()

collect_sources(SOURCE_BASE_CODEC_TESTS
    scale_reducer_unittest.cc)
